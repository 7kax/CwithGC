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
#include <cstring>
#include <unordered_map>
#include <utility>

namespace {

constexpr size_t heap_capacity = 4 * 1024;

struct ObjectHeader {
    const gc_ptr_table *pointer_table;
    size_t ref_count;
    size_t size;
};

class RefCountState {
  public:
    void init() noexcept {
        allocations_.clear();
        roots_.clear();
        free_bytes_ = heap_capacity;
        reclaimed_block_count_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) {
        require_initialized();

        size_t alloc_size;
        if (!gc_layout::block_size<ObjectHeader>(size, alloc_size) || alloc_size > heap_capacity ||
            alloc_size > free_bytes_)
            gc_allocation_failure();

        auto block = gc_runtime::malloc_bytes<ObjectHeader>(alloc_size);
        block->pointer_table = nullptr;
        block->ref_count = 0;
        block->size = alloc_size;

        void *payload = gc_layout::payload(block.get());
        std::memset(payload, 0, size);

        ObjectHeader *header = block.get();
        allocations_.emplace(header, std::move(block));
        free_bytes_ -= alloc_size;
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

    void register_object(void *object, const gc_ptr_table *pointer_table) {
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

    void assign_pointer(void *destination_slot, void *source) noexcept {
        require_initialized();

        auto **destination = static_cast<void **>(destination_slot);
        void *old = *destination;
        if (old == source)
            return;

        // Retain the new object before releasing the old one. The old object
        // may own source and recursively release it when its count reaches zero.
        if (source != nullptr)
            increment_ref_count(source);

        // Store before releasing old because destination may be a field inside old.
        *destination = source;
        if (old != nullptr)
            decrement_ref_count(old);
    }

    void collect() const noexcept {
        require_initialized();
        // Reference counting reclaims objects when their count reaches zero.
    }

    void end_scope(gc_scope_token token) noexcept {
        require_initialized();
        roots_.end_scope(token, [this](void *ptr) noexcept { decrement_ref_count(ptr); });
    }

    void cleanup() noexcept {
        allocations_.clear();
        roots_.clear();
        free_bytes_ = 0;
        reclaimed_block_count_ = 0;
        initialized_ = false;
    }

    size_t free_bytes() const noexcept { return free_bytes_; }

    size_t reclaimed_block_count() const noexcept { return reclaimed_block_count_; }

    size_t root_count() const noexcept { return roots_.size(); }

    bool initialized() const noexcept { return initialized_; }

#if CWITHGC_INTERNAL_INSPECTION
    gc_debug_memory_layout memory_layout() const {
        require_initialized();

        gc_layout::LayoutBuilder layout;
        for (const auto &allocation : allocations_)
            layout.add(allocation.first, allocation.first->size, GC_DEBUG_BLOCK_ALLOCATED);
        return layout.build();
    }
#endif

  private:
    static ObjectHeader *get_object_header(void *object) noexcept {
        return gc_layout::metadata<ObjectHeader>(object);
    }

    void require_initialized() const noexcept { gc_runtime::require_initialized(initialized_); }

    static void increment_ref_count(void *ptr) noexcept {
        assert(ptr != nullptr);
        get_object_header(ptr)->ref_count++;
    }

    void decrement_ref_count(void *ptr) noexcept {
        assert(ptr != nullptr);

        ObjectHeader *header = get_object_header(ptr);
        if (header->ref_count > 0)
            header->ref_count--;
        if (header->ref_count != 0)
            return;

        if (header->pointer_table != nullptr) {
            gc_pointer_table::for_each_field(*header->pointer_table, ptr,
                                             [this](void **child_ptr) noexcept {
                                                 if (*child_ptr != nullptr)
                                                     decrement_ref_count(*child_ptr);
                                             });
        }

        const auto allocation = allocations_.find(header);
        assert(allocation != allocations_.end());

        const size_t released_size = header->size;
        free_bytes_ += released_size;
        reclaimed_block_count_++;
        allocations_.erase(allocation);
    }

    bool initialized_ = false;
    size_t free_bytes_ = 0;
    gc_runtime::RootSet roots_;
    std::unordered_map<ObjectHeader *, gc_runtime::MallocPtr<ObjectHeader>> allocations_;
    size_t reclaimed_block_count_ = 0;
};

RefCountState state;

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

void gc_allocation_failure(void) noexcept {
    gc_runtime::fatal("Allocation failure");
}

void gc_collect(void) noexcept {
    state.collect();
}

void gc_cleanup(void) noexcept {
    state.cleanup();
}

void gc_pointer_assign(void *destination_slot, void *source) noexcept {
    state.assign_pointer(destination_slot, source);
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
        out->relocated_block_count = 0;
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
