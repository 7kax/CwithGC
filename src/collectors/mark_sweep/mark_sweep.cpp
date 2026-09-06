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
#include <optional>
#include <utility>

namespace {

constexpr size_t heap_capacity = 4 * 1024;

struct FreeBlock {
    size_t size;
    FreeBlock *next;
};

struct ObjectHeader {
    std::uint8_t marked;
    const gc_ptr_table *pointer_table;
    size_t size;
};

static_assert(heap_capacity % gc_layout::alignment == 0);
static_assert(gc_layout::alignment >= alignof(FreeBlock));

class FreeList {
  public:
    struct Allocation {
        std::byte *memory;
        size_t size;
    };

    void reset(std::byte *memory, size_t size) noexcept {
        assert(memory != nullptr);
        assert(size >= minimum_block_size);

        head_ = reinterpret_cast<FreeBlock *>(memory);
        head_->size = size;
        head_->next = nullptr;
        tail_ = nullptr;
        free_bytes_ = size;
    }

    void clear() noexcept {
        head_ = nullptr;
        tail_ = nullptr;
        free_bytes_ = 0;
    }

    std::optional<Allocation> allocate(size_t size) noexcept {
        assert(size > 0);

        FreeBlock *previous = nullptr;
        FreeBlock *current = head_;
        while (current != nullptr && current->size < size) {
            previous = current;
            current = current->next;
        }
        if (current == nullptr)
            return std::nullopt;

        const size_t current_size = current->size;
        const size_t remainder = current_size - size;
        if (remainder >= minimum_block_size) {
            auto *next =
                reinterpret_cast<FreeBlock *>(reinterpret_cast<std::byte *>(current) + size);
            next->size = remainder;
            next->next = current->next;
            replace(previous, next);

            free_bytes_ -= size;
            return Allocation{reinterpret_cast<std::byte *>(current), size};
        }

        replace(previous, current->next);
        free_bytes_ -= current_size;
        return Allocation{reinterpret_cast<std::byte *>(current), current_size};
    }

    FreeBlock *head() const noexcept { return head_; }

    size_t free_bytes() const noexcept { return free_bytes_; }

    void begin_rebuild() noexcept {
        head_ = nullptr;
        tail_ = nullptr;
        free_bytes_ = 0;
    }

    void append(std::byte *memory, size_t size) noexcept {
        assert(memory != nullptr);
        assert(size >= minimum_block_size);

        if (tail_ != nullptr && reinterpret_cast<std::byte *>(tail_) + tail_->size == memory) {
            tail_->size += size;
            free_bytes_ += size;
            return;
        }

        auto *block = reinterpret_cast<FreeBlock *>(memory);
        block->size = size;
        block->next = nullptr;

        if (tail_ == nullptr)
            head_ = block;
        else
            tail_->next = block;
        tail_ = block;
        free_bytes_ += size;
    }

  private:
    static constexpr size_t minimum_block_size = gc_layout::align_up(sizeof(FreeBlock));

    void replace(FreeBlock *previous, FreeBlock *replacement) noexcept {
        if (previous == nullptr)
            head_ = replacement;
        else
            previous->next = replacement;
    }

    FreeBlock *head_ = nullptr;
    FreeBlock *tail_ = nullptr;
    size_t free_bytes_ = 0;
};

class MarkSweepState {
  public:
    void init() {
        auto new_heap = gc_runtime::malloc_bytes(heap_capacity);

        roots_.clear();
        heap_ = std::move(new_heap);
        free_list_.reset(heap_.get(), heap_capacity);
        reclaimed_block_count_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) noexcept {
        require_initialized();

        size_t requested_size;
        if (!gc_layout::block_size<ObjectHeader>(size, requested_size) ||
            requested_size > heap_capacity)
            gc_runtime::allocation_failure();

        std::optional<FreeList::Allocation> allocation = free_list_.allocate(requested_size);
        if (!allocation.has_value()) {
            collect();
            allocation = free_list_.allocate(requested_size);
            if (!allocation.has_value())
                gc_runtime::allocation_failure();
        }

        auto *block = reinterpret_cast<ObjectHeader *>(allocation->memory);

        block->pointer_table = nullptr;
        block->size = allocation->size;
        block->marked = 0;

        void *payload = gc_layout::payload(block);
        std::memset(payload, 0, size);
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
        mark_phase();
        sweep_phase();
    }

    void end_scope(gc_scope_token token) noexcept {
        require_initialized();
        roots_.end_scope(token);
    }

    void cleanup() noexcept {
        heap_.reset();
        free_list_.clear();
        reclaimed_block_count_ = 0;
        roots_.clear();
        initialized_ = false;
    }

    size_t free_bytes() const noexcept { return free_list_.free_bytes(); }

    size_t reclaimed_block_count() const noexcept { return reclaimed_block_count_; }

    size_t root_count() const noexcept { return roots_.size(); }

    bool initialized() const noexcept { return initialized_; }

#if CWITHGC_INTERNAL_INSPECTION
    gc_debug_memory_layout memory_layout() const {
        require_initialized();

        gc_layout::LayoutBuilder layout;
        std::byte *heap_end = heap_.get() + heap_capacity;
        std::byte *scanning = heap_.get();
        FreeBlock *next_free_block = free_list_.head();

        while (scanning < heap_end) {
            if (next_free_block != nullptr &&
                scanning == reinterpret_cast<std::byte *>(next_free_block)) {
                const size_t size = next_free_block->size;
                layout.add(scanning, size, GC_DEBUG_BLOCK_FREE);
                scanning += size;
                next_free_block = next_free_block->next;
            } else {
                auto *header = reinterpret_cast<ObjectHeader *>(scanning);
                layout.add(scanning, header->size, GC_DEBUG_BLOCK_ALLOCATED);
                scanning += header->size;
            }
        }

        return layout.build();
    }
#endif

  private:
    static ObjectHeader *get_object_header(void *object) noexcept {
        return gc_layout::metadata<ObjectHeader>(object);
    }

    void require_initialized() const noexcept { gc_runtime::require_initialized(initialized_); }

    void mark(void *ptr) noexcept {
        ObjectHeader *header = get_object_header(ptr);
        if (header->marked)
            return;

        header->marked = 1;
        if (header->pointer_table == nullptr)
            return;

        gc_pointer_table::for_each_field(*header->pointer_table, ptr,
                                         [this](void **child_ptr) noexcept {
                                             if (*child_ptr != nullptr)
                                                 mark(*child_ptr);
                                         });
    }

    void mark_phase() noexcept {
        roots_.for_each([this](void **root_slot) noexcept {
            if (*root_slot != nullptr)
                mark(*root_slot);
        });
    }

    void sweep_phase() noexcept {
        std::byte *heap_end = heap_.get() + heap_capacity;
        std::byte *sweeping = heap_.get();
        FreeBlock *next_free_block = free_list_.head();
        free_list_.begin_rebuild();

        while (sweeping < heap_end) {
            if (next_free_block != nullptr &&
                sweeping == reinterpret_cast<std::byte *>(next_free_block)) {
                const size_t block_size = next_free_block->size;
                FreeBlock *following_free_block = next_free_block->next;
                free_list_.append(sweeping, block_size);
                sweeping += block_size;
                next_free_block = following_free_block;
                continue;
            }

            assert(next_free_block == nullptr ||
                   sweeping < reinterpret_cast<std::byte *>(next_free_block));
            auto *header = reinterpret_cast<ObjectHeader *>(sweeping);
            const size_t block_size = header->size;

            if (header->marked == 0) {
                free_list_.append(sweeping, block_size);
                reclaimed_block_count_++;
            } else {
                header->marked = 0;
            }

            sweeping += block_size;
        }
    }

    bool initialized_ = false;
    gc_runtime::MallocPtr<> heap_;
    gc_runtime::RootSet roots_;
    FreeList free_list_;
    size_t reclaimed_block_count_ = 0;
};

MarkSweepState state;

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
