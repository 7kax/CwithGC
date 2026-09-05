#include "common/layout.hpp"
#include "common/pointer_table.hpp"
#include "common/root_set.hpp"
#include "common/runtime.hpp"
#include "gc.h"

#ifdef GC_DEBUG
#include "common/memory_layout.hpp"
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

namespace {

constexpr size_t heap_size = 4 * 1024;

struct free_block {
    size_t size;
    free_block *next;
};

struct meta_data {
    std::uint8_t marked;
    const gc_ptr_table *ptr_table;
    size_t size;
};

static_assert(heap_size % gc_layout::alignment == 0);
static_assert(gc_layout::alignment >= alignof(free_block));

class FreeList {
  public:
    struct Allocation {
        std::byte *memory;
        size_t size;
    };

    void reset(std::byte *memory, size_t size) noexcept {
        assert(memory != nullptr);
        assert(size >= minimum_block_size);

        head_ = reinterpret_cast<free_block *>(memory);
        head_->size = size;
        head_->next = nullptr;
        tail_ = nullptr;
        free_size_ = size;
    }

    void clear() noexcept {
        head_ = nullptr;
        tail_ = nullptr;
        free_size_ = 0;
    }

    std::optional<Allocation> allocate(size_t size) noexcept {
        assert(size > 0);

        free_block *previous = nullptr;
        free_block *current = head_;
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
                reinterpret_cast<free_block *>(reinterpret_cast<std::byte *>(current) + size);
            next->size = remainder;
            next->next = current->next;
            replace(previous, next);

            free_size_ -= size;
            return Allocation{reinterpret_cast<std::byte *>(current), size};
        }

        replace(previous, current->next);
        free_size_ -= current_size;
        return Allocation{reinterpret_cast<std::byte *>(current), current_size};
    }

    free_block *head() const noexcept { return head_; }

    size_t free_size() const noexcept { return free_size_; }

    void begin_rebuild() noexcept {
        head_ = nullptr;
        tail_ = nullptr;
        free_size_ = 0;
    }

    void append(std::byte *memory, size_t size) noexcept {
        assert(memory != nullptr);
        assert(size >= minimum_block_size);

        if (tail_ != nullptr && reinterpret_cast<std::byte *>(tail_) + tail_->size == memory) {
            tail_->size += size;
            free_size_ += size;
            return;
        }

        auto *block = reinterpret_cast<free_block *>(memory);
        block->size = size;
        block->next = nullptr;

        if (tail_ == nullptr)
            head_ = block;
        else
            tail_->next = block;
        tail_ = block;
        free_size_ += size;
    }

  private:
    static constexpr size_t minimum_block_size = gc_layout::align_up(sizeof(free_block));

    void replace(free_block *previous, free_block *replacement) noexcept {
        if (previous == nullptr)
            head_ = replacement;
        else
            previous->next = replacement;
    }

    free_block *head_ = nullptr;
    free_block *tail_ = nullptr;
    size_t free_size_ = 0;
};

class MarkSweepState {
  public:
    void init() {
        auto new_heap = gc_runtime::malloc_bytes(heap_size);

        roots_.clear();
        heap_ = std::move(new_heap);
        free_list_.reset(heap_.get(), heap_size);
        block_collected_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) noexcept {
        require_initialized();

        size_t requested_size;
        if (!gc_layout::block_size<meta_data>(size, requested_size) || requested_size > heap_size)
            gc_allocation_failure();

        std::optional<FreeList::Allocation> allocation = free_list_.allocate(requested_size);
        if (!allocation.has_value()) {
            collect();
            allocation = free_list_.allocate(requested_size);
            if (!allocation.has_value())
                gc_allocation_failure();
        }

        auto *block = reinterpret_cast<meta_data *>(allocation->memory);

        block->ptr_table = nullptr;
        block->size = allocation->size;
        block->marked = 0;

        void *payload = gc_layout::payload(block);
        std::memset(payload, 0, size);
        return payload;
    }

    void add_root(void *ptr_address, void *frame_address) {
        require_initialized();

        roots_.add(ptr_address, frame_address);
    }

    void register_object(void *ptr, const gc_ptr_table *ptr_map) const noexcept {
        require_initialized();

        meta_data *meta_ptr = get_meta_data(ptr);
        assert(meta_ptr->ptr_table == nullptr);

        size_t payload_capacity;
        if (ptr_map == nullptr ||
            !gc_layout::payload_capacity<meta_data>(meta_ptr->size, payload_capacity) ||
            !gc_pointer_table::valid_for_payload(*ptr_map, payload_capacity))
            gc_runtime::invalid_pointer_table();

        meta_ptr->ptr_table = ptr_map;
    }

    void copy_pointer(void *dst_address, void *src) const noexcept {
        require_initialized();
        *static_cast<void **>(dst_address) = src;
    }

    void collect() noexcept {
        require_initialized();
        mark_phase();
        sweep_phase();
    }

    void pop_roots() noexcept {
        require_initialized();
        roots_.pop_frame();
    }

    void cleanup() noexcept {
        heap_.reset();
        free_list_.clear();
        block_collected_ = 0;
        roots_.clear();
        initialized_ = false;
    }

    size_t free_size() const noexcept { return free_list_.free_size(); }

    size_t block_collected() const noexcept { return block_collected_; }

    size_t root_size() const noexcept { return roots_.size(); }

#ifdef GC_DEBUG
    mem_block_info *memory_layout() const {
        require_initialized();

        gc_layout::layout_builder layout;
        std::byte *heap_end = heap_.get() + heap_size;
        std::byte *scanning = heap_.get();
        free_block *next_free_block = free_list_.head();

        while (scanning < heap_end) {
            if (next_free_block != nullptr &&
                scanning == reinterpret_cast<std::byte *>(next_free_block)) {
                const size_t size = next_free_block->size;
                layout.add(scanning, size, true);
                scanning += size;
                next_free_block = next_free_block->next;
            } else {
                auto *meta_ptr = reinterpret_cast<meta_data *>(scanning);
                layout.add(scanning, meta_ptr->size, false);
                scanning += meta_ptr->size;
            }
        }

        return layout.release();
    }
#endif

  private:
    static meta_data *get_meta_data(void *ptr) noexcept {
        return gc_layout::metadata<meta_data>(ptr);
    }

    void require_initialized() const noexcept { gc_runtime::require_initialized(initialized_); }

    void mark(void *ptr) noexcept {
        meta_data *meta_ptr = get_meta_data(ptr);
        if (meta_ptr->marked)
            return;

        meta_ptr->marked = 1;
        if (meta_ptr->ptr_table == nullptr)
            return;

        gc_pointer_table::for_each_field(*meta_ptr->ptr_table, ptr,
                                         [this](void **child_ptr) noexcept {
                                             if (*child_ptr != nullptr)
                                                 mark(*child_ptr);
                                         });
    }

    void mark_phase() noexcept {
        roots_.for_each([this](void **ptr) noexcept {
            if (*ptr != nullptr)
                mark(*ptr);
        });
    }

    void sweep_phase() noexcept {
        std::byte *heap_end = heap_.get() + heap_size;
        std::byte *sweeping = heap_.get();
        free_block *next_free_block = free_list_.head();
        free_list_.begin_rebuild();

        while (sweeping < heap_end) {
            if (next_free_block != nullptr &&
                sweeping == reinterpret_cast<std::byte *>(next_free_block)) {
                const size_t block_size = next_free_block->size;
                free_block *following_free_block = next_free_block->next;
                free_list_.append(sweeping, block_size);
                sweeping += block_size;
                next_free_block = following_free_block;
                continue;
            }

            assert(next_free_block == nullptr ||
                   sweeping < reinterpret_cast<std::byte *>(next_free_block));
            auto *meta_ptr = reinterpret_cast<meta_data *>(sweeping);
            const size_t block_size = meta_ptr->size;

            if (meta_ptr->marked == 0) {
                free_list_.append(sweeping, block_size);
                block_collected_++;
            } else {
                meta_ptr->marked = 0;
            }

            sweeping += block_size;
        }
    }

    bool initialized_ = false;
    gc_runtime::malloc_ptr<> heap_;
    gc_runtime::root_set roots_;
    FreeList free_list_;
    size_t block_collected_ = 0;
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

void gc_local_var(void *ptr_address) noexcept try {
    state.add_root(ptr_address, __builtin_frame_address(1));
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_register(void *ptr, const gc_ptr_table *ptr_map) noexcept try {
    state.register_object(ptr, ptr_map);
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_allocation_failure(void) noexcept {
    gc_runtime::fatal("Allocation failed");
}

void gc_collect(void) noexcept {
    state.collect();
}

void gc_cleanup(void) noexcept {
    state.cleanup();
}

void gc_ptr_copy(void *dst_address, void *src) noexcept {
    state.copy_pointer(dst_address, src);
}

void gc_pop(void) noexcept {
    state.pop_roots();
}

#ifdef GC_DEBUG
size_t gc_heap_size(void) noexcept {
    return heap_size;
}

size_t gc_free_size(void) noexcept {
    return state.free_size();
}

size_t gc_block_collected(void) noexcept {
    return state.block_collected();
}

size_t gc_meta_size(void) noexcept {
    return gc_layout::header_size<meta_data>;
}

size_t gc_root_size(void) noexcept {
    return state.root_size();
}

mem_block_info *gc_mem_layout(void) noexcept try { return state.memory_layout(); } catch (...) {
    gc_runtime::handle_current_exception();
}
#endif

} // extern "C"
