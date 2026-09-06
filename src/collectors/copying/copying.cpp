#include "common/layout.hpp"
#include "common/pointer_table.hpp"
#include "common/root_set.hpp"
#include "common/runtime.hpp"
#include "gc.h"
#include "gc_debug.h"

#if CWITHGC_INTERNAL_INSPECTION
#include "common/memory_layout.hpp"
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace {

constexpr size_t heap_capacity = 4 * 1024;

struct ObjectHeader {
    std::uint8_t copied;
    const gc_ptr_table *pointer_table;
    size_t size;
    void *forwarding;
};

static_assert(heap_capacity % gc_layout::alignment == 0);

class Semispace {
  public:
    void reset(gc_runtime::MallocPtr<> memory) noexcept {
        memory_ = std::move(memory);
        top_ = memory_.get();
    }

    void clear() noexcept {
        memory_.reset();
        top_ = nullptr;
    }

    void rewind() noexcept {
        assert(memory_ != nullptr);
        top_ = memory_.get();
    }

    std::byte *allocate(size_t size) noexcept {
        assert(size <= available());
        std::byte *allocation = top_;
        top_ += size;
        return allocation;
    }

    std::byte *begin() const noexcept { return memory_.get(); }

    std::byte *top() const noexcept { return top_; }

    size_t used() const noexcept {
        assert(memory_ != nullptr);
        return static_cast<size_t>(top_ - memory_.get());
    }

    size_t available() const noexcept { return heap_capacity - used(); }

    friend void swap(Semispace &lhs, Semispace &rhs) noexcept {
        using std::swap;
        swap(lhs.memory_, rhs.memory_);
        swap(lhs.top_, rhs.top_);
    }

  private:
    gc_runtime::MallocPtr<> memory_;
    std::byte *top_ = nullptr;
};

class CopyingState {
  public:
    void init() {
        auto new_from = gc_runtime::malloc_bytes(heap_capacity);
        auto new_to = gc_runtime::malloc_bytes(heap_capacity);

        roots_.clear();
        from_space_.reset(std::move(new_from));
        to_space_.reset(std::move(new_to));
        allocated_block_count_ = 0;
        reclaimed_block_count_ = 0;
        relocated_block_count_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) {
        require_initialized();

        size_t alloc_size;
        if (!gc_layout::block_size<ObjectHeader>(size, alloc_size) || alloc_size > heap_capacity)
            gc_runtime::allocation_failure();

        if (alloc_size > from_space_.available())
            collect();
        if (alloc_size > from_space_.available())
            gc_runtime::allocation_failure();

        auto *block = reinterpret_cast<ObjectHeader *>(from_space_.allocate(alloc_size));

        block->copied = 0;
        block->forwarding = nullptr;
        block->pointer_table = nullptr;
        block->size = alloc_size;

        void *payload = gc_layout::payload(block);
        std::memset(payload, 0, size);
        allocated_block_count_++;
        return payload;
    }

    gc_scope_token begin_scope() {
        require_initialized();

        return roots_.begin_scope();
    }

    void add_root(void *root_slot) {
        require_initialized();

        roots_.add(root_slot);
    }

    void register_object(void *object, const gc_ptr_table *pointer_table) const noexcept {
        require_initialized();

        ObjectHeader *header = get_object_header(object);
        assert(header->pointer_table == nullptr);

        size_t payload_capacity;
        if (pointer_table == nullptr ||
            !gc_layout::payload_capacity<ObjectHeader>(header->size, payload_capacity) ||
            !gc_pointer_table::valid_for_payload(*pointer_table, payload_capacity))
            gc_runtime::invalid_pointer_table();

        header->pointer_table = pointer_table;
    }

    void assign_pointer(void *destination_slot, void *source) const noexcept {
        require_initialized();
        *static_cast<void **>(destination_slot) = source;
    }

    void collect() noexcept {
        require_initialized();

        const size_t relocated_before_collection = relocated_block_count_;
        to_space_.rewind();
        std::byte *scan = to_space_.begin();

        roots_.for_each([this](void **root_slot) noexcept {
            if (*root_slot != nullptr)
                *root_slot = evacuate(*root_slot);
        });

        // Objects copied while scanning are appended to to-space, so the
        // to-space itself acts as the breadth-first work queue.
        while (scan < to_space_.top()) {
            auto *header = reinterpret_cast<ObjectHeader *>(scan);
            const size_t block_size = header->size;
            scan_object(header);
            scan += block_size;
        }

        const size_t relocated_this_collection =
            relocated_block_count_ - relocated_before_collection;
        assert(relocated_this_collection <= allocated_block_count_);
        reclaimed_block_count_ += allocated_block_count_ - relocated_this_collection;
        allocated_block_count_ = relocated_this_collection;
        swap(from_space_, to_space_);
    }

    void end_scope(gc_scope_token token) noexcept {
        require_initialized();
        roots_.end_scope(token);
    }

    void cleanup() noexcept {
        from_space_.clear();
        to_space_.clear();
        allocated_block_count_ = 0;
        reclaimed_block_count_ = 0;
        relocated_block_count_ = 0;
        roots_.clear();
        initialized_ = false;
    }

    size_t free_bytes() const noexcept { return initialized_ ? from_space_.available() : 0; }

    size_t reclaimed_block_count() const noexcept { return reclaimed_block_count_; }

    size_t relocated_block_count() const noexcept { return relocated_block_count_; }

    size_t root_count() const noexcept { return roots_.size(); }

    bool initialized() const noexcept { return initialized_; }

#if CWITHGC_INTERNAL_INSPECTION
    gc_debug_memory_layout memory_layout() const {
        require_initialized();

        gc_layout::LayoutBuilder layout;
        std::byte *current = from_space_.begin();
        while (current < from_space_.top()) {
            auto *header = reinterpret_cast<ObjectHeader *>(current);
            layout.add(current, header->size, GC_DEBUG_BLOCK_ALLOCATED);
            current += header->size;
        }
        layout.add(from_space_.top(), from_space_.available(), GC_DEBUG_BLOCK_FREE);
        return layout.build();
    }
#endif

  private:
    static ObjectHeader *get_object_header(void *object) noexcept {
        return gc_layout::metadata<ObjectHeader>(object);
    }

    void require_initialized() const noexcept { gc_runtime::require_initialized(initialized_); }

    void *evacuate(void *object) noexcept {
        ObjectHeader *old_header = get_object_header(object);
        if (old_header->copied) {
            assert(old_header->forwarding != nullptr);
            return old_header->forwarding;
        }

        const size_t block_size = old_header->size;
        auto *new_header = reinterpret_cast<ObjectHeader *>(to_space_.allocate(block_size));
        std::memcpy(new_header, old_header, block_size);
        void *new_payload = gc_layout::payload(new_header);
        relocated_block_count_++;

        old_header->copied = 1;
        old_header->forwarding = new_payload;
        new_header->copied = 0;
        new_header->forwarding = nullptr;
        return new_payload;
    }

    void scan_object(ObjectHeader *header) noexcept {
        const gc_ptr_table *pointer_table = header->pointer_table;
        if (pointer_table == nullptr)
            return;

        gc_pointer_table::for_each_field(*pointer_table, gc_layout::payload(header),
                                         [this](void **child_slot) noexcept {
                                             if (*child_slot != nullptr)
                                                 *child_slot = evacuate(*child_slot);
                                         });
    }

    bool initialized_ = false;
    Semispace from_space_;
    Semispace to_space_;
    gc_runtime::RootSet roots_;
    size_t allocated_block_count_ = 0;
    size_t reclaimed_block_count_ = 0;
    size_t relocated_block_count_ = 0;
};

CopyingState state;

} // namespace

extern "C" {

void gc_init(void) noexcept try { state.init(); } catch (...) {
    gc_runtime::handle_current_exception();
}

void *gc_malloc(size_t size) noexcept try { return state.allocate(size); } catch (...) {
    gc_runtime::handle_current_exception();
}

gc_scope_token gc_scope_begin(void) noexcept try { return state.begin_scope(); } catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_scope_end(gc_scope_token token) noexcept try { state.end_scope(token); } catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_scope_add_root(void *root_slot) noexcept try { state.add_root(root_slot); } catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_register_object(void *object, const gc_ptr_table *pointer_table) noexcept try {
    state.register_object(object, pointer_table);
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_pointer_assign(void *destination_slot, void *source) noexcept {
    state.assign_pointer(destination_slot, source);
}

void gc_collect(void) noexcept {
    state.collect();
}

void gc_cleanup(void) noexcept {
    state.cleanup();
}

int gc_debug_is_available(void) noexcept {
#if CWITHGC_INTERNAL_INSPECTION
    return 1;
#else
    return 0;
#endif
}

gc_debug_status gc_debug_get_stats(gc_debug_stats *out) noexcept {
    if (out == nullptr)
        return GC_DEBUG_INVALID_ARGUMENT;
    *out = {};

#if CWITHGC_INTERNAL_INSPECTION
    if (!state.initialized())
        return GC_DEBUG_NOT_INITIALIZED;

    try {
        out->heap_capacity = heap_capacity;
        out->free_bytes = state.free_bytes();
        out->reclaimed_block_count = state.reclaimed_block_count();
        out->relocated_block_count = state.relocated_block_count();
        out->metadata_size = gc_layout::header_size<ObjectHeader>;
        out->root_count = state.root_count();
        return GC_DEBUG_OK;
    } catch (const std::bad_alloc &) {
        *out = {};
        return GC_DEBUG_OUT_OF_MEMORY;
    } catch (...) {
        *out = {};
        return GC_DEBUG_INTERNAL_ERROR;
    }
#else
    return GC_DEBUG_UNAVAILABLE;
#endif
}

gc_debug_status gc_debug_snapshot_memory_layout(gc_debug_memory_layout *out) noexcept {
    if (out == nullptr)
        return GC_DEBUG_INVALID_ARGUMENT;
    *out = {};

#if CWITHGC_INTERNAL_INSPECTION
    if (!state.initialized())
        return GC_DEBUG_NOT_INITIALIZED;

    try {
        *out = state.memory_layout();
        return GC_DEBUG_OK;
    } catch (const std::bad_alloc &) {
        *out = {};
        return GC_DEBUG_OUT_OF_MEMORY;
    } catch (...) {
        *out = {};
        return GC_DEBUG_INTERNAL_ERROR;
    }
#else
    return GC_DEBUG_UNAVAILABLE;
#endif
}

} // extern "C"
