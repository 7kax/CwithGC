#include "common/layout.hpp"
#include "common/pointer_table.hpp"
#include "common/runtime.hpp"
#include "gc.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace {

constexpr size_t heap_size = 4 * 1024;

struct stack_ptr {
    void **ptr;
    void *frame;
};

struct free_block {
    size_t size;
    free_block *next;
};

struct meta_data {
    u_int8_t marked;
    const gc_ptr_table *ptr_table;
    size_t size;
};

static_assert(heap_size % gc_layout::alignment == 0);
static_assert(gc_layout::alignment >= alignof(free_block));

constexpr size_t minimum_free_block_size = gc_layout::align_up(sizeof(free_block));

[[noreturn]] void invalid_pointer_table() {
    gc_runtime::fatal("Invalid pointer table");
}

class MarkSweepState {
  public:
    void init() {
        auto new_heap = gc_runtime::malloc_bytes(heap_size);

        release_roots();
        heap_ = std::move(new_heap);
        free_list_ = reinterpret_cast<free_block *>(heap_.get());
        free_list_->size = heap_size;
        free_list_->next = nullptr;
        free_size_ = heap_size;
        block_collected_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) noexcept {
        require_initialized();

        size_t requested_size;
        if (!gc_layout::block_size<meta_data>(size, requested_size) || requested_size > heap_size)
            gc_allocation_failure();

        size_t allocated_size;
        auto *block = static_cast<meta_data *>(pick_free_block(requested_size, allocated_size));
        if (block == nullptr)
            gc_allocation_failure();

        block->ptr_table = nullptr;
        block->size = allocated_size;
        block->marked = 0;

        void *payload = gc_layout::payload(block);
        std::memset(payload, 0, size);
        return payload;
    }

    void add_root(void *ptr_address, void *frame_address) {
        require_initialized();

        auto **ptr = static_cast<void **>(ptr_address);
        roots_.push_back({ptr, frame_address});
        *ptr = nullptr;
    }

    void register_object(void *ptr, const gc_ptr_table *ptr_map) const noexcept {
        require_initialized();

        meta_data *meta_ptr = get_meta_data(ptr);
        assert(meta_ptr->ptr_table == nullptr);

        if (ptr_map == nullptr)
            invalid_pointer_table();
        assert(ptr_map->array_len > 0);
        assert(ptr_map->struct_size > 0);
        assert(!ptr_map->positions.empty());

        size_t payload_size;
        const size_t payload_capacity = meta_ptr->size - gc_layout::header_size<meta_data>;
        if (!gc_layout::checked_mul(ptr_map->array_len, ptr_map->struct_size, payload_size) ||
            payload_size > payload_capacity)
            invalid_pointer_table();

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
        if (roots_.empty())
            return;

        void *frame_address = roots_.back().frame;
        while (!roots_.empty() && roots_.back().frame == frame_address)
            roots_.pop_back();
    }

    void cleanup() noexcept {
        heap_.reset();
        free_list_ = nullptr;
        free_size_ = 0;
        block_collected_ = 0;
        release_roots();
        initialized_ = false;
    }

    size_t free_size() const noexcept { return free_size_; }

    size_t block_collected() const noexcept { return block_collected_; }

    size_t root_size() const noexcept { return roots_.size(); }

    mem_block_info *memory_layout() const {
        require_initialized();

        std::vector<mem_block_info> mem_blocks;
        std::byte *heap_end = heap_.get() + heap_size;
        std::byte *scanning = heap_.get();
        free_block *next_free_block = free_list_;

        while (scanning < heap_end) {
            if (next_free_block != nullptr &&
                scanning == reinterpret_cast<std::byte *>(next_free_block)) {
                const size_t size = next_free_block->size;
                mem_blocks.push_back({scanning, size, 1});
                scanning += size;
                next_free_block = next_free_block->next;
            } else {
                auto *meta_ptr = reinterpret_cast<meta_data *>(scanning);
                mem_blocks.push_back({scanning, meta_ptr->size, 0});
                scanning += meta_ptr->size;
            }
        }

        auto *layout = new mem_block_info[mem_blocks.size() + 1];
        std::copy(mem_blocks.begin(), mem_blocks.end(), layout);
        layout[mem_blocks.size()] = {nullptr, 0, 0};
        return layout;
    }

  private:
    static meta_data *get_meta_data(void *ptr) noexcept {
        return gc_layout::metadata<meta_data>(ptr);
    }

    void require_initialized() const noexcept {
        if (!initialized_)
            gc_runtime::fatal("GC is not initialized");
    }

    void *pick_free_block(size_t size, size_t &allocated_size) noexcept {
        assert(size > 0);

        free_block *previous = nullptr;
        free_block *current = free_list_;
        while (current != nullptr && current->size < size) {
            previous = current;
            current = current->next;
        }

        if (current == nullptr) {
            collect();
            previous = nullptr;
            current = free_list_;
            while (current != nullptr && current->size < size) {
                previous = current;
                current = current->next;
            }
            if (current == nullptr)
                return nullptr;
        }

        const size_t current_size = current->size;
        const size_t remainder = current_size - size;
        if (remainder >= minimum_free_block_size) {
            auto *new_block =
                reinterpret_cast<free_block *>(reinterpret_cast<std::byte *>(current) + size);
            new_block->size = remainder;
            new_block->next = current->next;

            if (previous == nullptr)
                free_list_ = new_block;
            else
                previous->next = new_block;

            free_size_ -= size;
            allocated_size = size;
            return current;
        }

        if (previous == nullptr)
            free_list_ = current->next;
        else
            previous->next = current->next;

        free_size_ -= current_size;
        allocated_size = current_size;
        return current;
    }

    void mark(void *ptr) noexcept {
        meta_data *meta_ptr = get_meta_data(ptr);
        if (meta_ptr->marked)
            return;

        meta_ptr->marked = 1;
        if (meta_ptr->ptr_table == nullptr)
            return;

        auto *cur_struct = static_cast<std::byte *>(ptr);
        for (size_t i = 0; i < meta_ptr->ptr_table->array_len; i++) {
            for (size_t position : meta_ptr->ptr_table->positions) {
                auto **child_ptr = reinterpret_cast<void **>(cur_struct + position);
                if (*child_ptr != nullptr)
                    mark(*child_ptr);
            }
            cur_struct += meta_ptr->ptr_table->struct_size;
        }
    }

    void mark_phase() noexcept {
        for (auto [ptr, frame] : roots_) {
            (void)frame;
            if (*ptr != nullptr)
                mark(*ptr);
        }
    }

    void sweep_phase() noexcept {
        std::byte *heap_end = heap_.get() + heap_size;
        std::byte *sweeping = heap_.get();
        free_block *next_free_block = free_list_;
        free_block *previous_free_block = nullptr;

        while (sweeping < heap_end) {
            if (next_free_block != nullptr &&
                sweeping == reinterpret_cast<std::byte *>(next_free_block)) {
                sweeping += next_free_block->size;
                previous_free_block = next_free_block;
                next_free_block = next_free_block->next;
                continue;
            }

            assert(next_free_block == nullptr ||
                   sweeping < reinterpret_cast<std::byte *>(next_free_block));
            auto *meta_ptr = reinterpret_cast<meta_data *>(sweeping);
            const size_t block_size = meta_ptr->size;

            if (meta_ptr->marked == 0) {
                auto *new_free_block = reinterpret_cast<free_block *>(sweeping);
                new_free_block->size = block_size;
                new_free_block->next = next_free_block;

                if (previous_free_block != nullptr)
                    previous_free_block->next = new_free_block;
                else
                    free_list_ = new_free_block;
                previous_free_block = new_free_block;

                free_size_ += block_size;
                block_collected_++;
            } else {
                meta_ptr->marked = 0;
            }

            sweeping += block_size;
        }

        for (free_block *current = free_list_; current != nullptr; current = current->next) {
            free_block *next = current->next;
            while (next != nullptr && reinterpret_cast<std::byte *>(current) + current->size ==
                                          reinterpret_cast<std::byte *>(next)) {
                current->size += next->size;
                current->next = next = next->next;
            }
        }
    }

    void release_roots() noexcept {
        std::vector<stack_ptr> empty;
        roots_.swap(empty);
    }

    bool initialized_ = false;
    gc_runtime::malloc_ptr<> heap_;
    std::vector<stack_ptr> roots_;
    free_block *free_list_ = nullptr;
    size_t free_size_ = 0;
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
