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

struct meta_data {
    u_int8_t copied;
    const gc_ptr_table *ptr_table;
    size_t size;
    void *forwarding;
};

static_assert(heap_size % gc_layout::alignment == 0);

[[noreturn]] void invalid_pointer_table() {
    gc_runtime::fatal("Invalid pointer table");
}

class CopyingState {
  public:
    void init() {
        auto new_from = gc_runtime::malloc_bytes(heap_size);
        auto new_to = gc_runtime::malloc_bytes(heap_size);

        release_roots();
        from_ = std::move(new_from);
        to_ = std::move(new_to);
        free_space_ = from_.get();
        free_size_ = heap_size;
        block_collected_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) {
        require_initialized();

        size_t alloc_size;
        if (!gc_layout::block_size<meta_data>(size, alloc_size) || alloc_size > heap_size)
            gc_allocation_failure();

        if (alloc_size > free_size_)
            collect();
        if (alloc_size > free_size_)
            gc_allocation_failure();

        auto *block = reinterpret_cast<meta_data *>(free_space_);
        free_space_ += alloc_size;
        free_size_ -= alloc_size;

        block->copied = 0;
        block->forwarding = nullptr;
        block->ptr_table = nullptr;
        block->size = alloc_size;

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

        free_space_ = to_.get();
        std::byte *scan = to_.get();

        for (auto [ptr_address, frame] : roots_) {
            (void)frame;
            if (*ptr_address != nullptr)
                *ptr_address = evacuate(*ptr_address);
        }

        // Objects copied while scanning are appended at free_space, so the
        // to-space itself acts as the breadth-first work queue.
        while (scan < free_space_) {
            auto *meta_ptr = reinterpret_cast<meta_data *>(scan);
            const size_t block_size = meta_ptr->size;
            scan_object(meta_ptr);
            scan += block_size;
        }

        std::swap(from_, to_);
        free_size_ = heap_size - static_cast<size_t>(free_space_ - from_.get());
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
        from_.reset();
        to_.reset();
        free_space_ = nullptr;
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

        std::vector<mem_block_info> mem_layout;
        std::byte *current = from_.get();
        while (current < free_space_) {
            auto *meta_ptr = reinterpret_cast<meta_data *>(current);
            mem_layout.push_back({current, meta_ptr->size, 0});
            current += meta_ptr->size;
        }
        mem_layout.push_back({free_space_, free_size_, 1});

        auto *layout = new mem_block_info[mem_layout.size() + 1];
        std::copy(mem_layout.begin(), mem_layout.end(), layout);
        layout[mem_layout.size()] = {nullptr, 0, 0};
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

    void *evacuate(void *ptr) noexcept {
        meta_data *old_meta = get_meta_data(ptr);
        if (old_meta->copied) {
            assert(old_meta->forwarding != nullptr);
            return old_meta->forwarding;
        }

        const size_t block_size = old_meta->size;
        auto *new_meta = reinterpret_cast<meta_data *>(free_space_);
        std::memcpy(new_meta, old_meta, block_size);
        void *new_payload = gc_layout::payload(new_meta);
        free_space_ += block_size;
        block_collected_++;

        old_meta->copied = 1;
        old_meta->forwarding = new_payload;
        new_meta->copied = 0;
        new_meta->forwarding = nullptr;
        return new_payload;
    }

    void scan_object(meta_data *meta_ptr) noexcept {
        const gc_ptr_table *ptr_table = meta_ptr->ptr_table;
        if (ptr_table == nullptr)
            return;

        auto *cur_struct = static_cast<std::byte *>(gc_layout::payload(meta_ptr));
        for (size_t i = 0; i < ptr_table->array_len; i++) {
            for (size_t position : ptr_table->positions) {
                auto **child_ptr = reinterpret_cast<void **>(cur_struct + position);
                if (*child_ptr != nullptr)
                    *child_ptr = evacuate(*child_ptr);
            }
            cur_struct += ptr_table->struct_size;
        }
    }

    void release_roots() noexcept {
        std::vector<stack_ptr> empty;
        roots_.swap(empty);
    }

    bool initialized_ = false;
    gc_runtime::malloc_ptr<> from_;
    gc_runtime::malloc_ptr<> to_;
    std::byte *free_space_ = nullptr;
    size_t free_size_ = 0;
    std::vector<stack_ptr> roots_;
    size_t block_collected_ = 0;
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

void gc_ptr_copy(void *dst_address, void *src) noexcept {
    state.copy_pointer(dst_address, src);
}

void gc_collect(void) noexcept {
    state.collect();
}

void gc_pop(void) noexcept {
    state.pop_roots();
}

void gc_cleanup(void) noexcept {
    state.cleanup();
}

void gc_allocation_failure(void) noexcept {
    gc_runtime::fatal("Allocation failure");
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
