#include "common/layout.hpp"
#include "common/pointer_table.hpp"
#include "common/root_set.hpp"
#include "common/runtime.hpp"
#include "gc.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace {

constexpr size_t heap_size = 4 * 1024;

struct meta_data {
    const gc_ptr_table *ptr_table;
    size_t ref_count;
    size_t size;
};

class RefCountState {
  public:
    void init() noexcept {
        allocations_.clear();
        roots_.clear();
        free_size_ = heap_size;
        block_collected_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) {
        require_initialized();

        size_t alloc_size;
        if (!gc_layout::block_size<meta_data>(size, alloc_size) || alloc_size > heap_size ||
            alloc_size > free_size_)
            gc_allocation_failure();

        auto block = gc_runtime::malloc_bytes<meta_data>(alloc_size);
        block->ptr_table = nullptr;
        block->ref_count = 0;
        block->size = alloc_size;

        void *payload = gc_layout::payload(block.get());
        std::memset(payload, 0, size);

        meta_data *meta_ptr = block.get();
        allocations_.emplace(meta_ptr, std::move(block));
        free_size_ -= alloc_size;
        return payload;
    }

    void add_root(void *ptr_address, void *frame_address) {
        require_initialized();

        roots_.add(ptr_address, frame_address);
    }

    void register_object(void *ptr, const gc_ptr_table *ptr_map) {
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

    void copy_pointer(void *dst_address, void *src) noexcept {
        require_initialized();

        auto **dst = static_cast<void **>(dst_address);
        void *old = *dst;
        if (old == src)
            return;

        // Retain the new object before releasing the old one. The old object
        // may own src and recursively release it when its count reaches zero.
        if (src != nullptr)
            increment_ref_count(src);

        // Store before releasing old because dst may be a field inside old.
        *dst = src;
        if (old != nullptr)
            decrement_ref_count(old);
    }

    void collect() const noexcept {
        require_initialized();
        // Reference counting reclaims objects when their count reaches zero.
    }

    void pop_roots() noexcept {
        require_initialized();
        roots_.pop_frame([this](void *ptr) noexcept { decrement_ref_count(ptr); });
    }

    void cleanup() noexcept {
        allocations_.clear();
        roots_.clear();
        free_size_ = 0;
        block_collected_ = 0;
        initialized_ = false;
    }

    size_t free_size() const noexcept { return free_size_; }

    size_t block_collected() const noexcept { return block_collected_; }

    size_t root_size() const noexcept { return roots_.size(); }

  private:
    static meta_data *get_meta_data(void *ptr) noexcept {
        return gc_layout::metadata<meta_data>(ptr);
    }

    void require_initialized() const noexcept { gc_runtime::require_initialized(initialized_); }

    static void increment_ref_count(void *ptr) noexcept {
        assert(ptr != nullptr);
        get_meta_data(ptr)->ref_count++;
    }

    void decrement_ref_count(void *ptr) noexcept {
        assert(ptr != nullptr);

        meta_data *meta_ptr = get_meta_data(ptr);
        if (meta_ptr->ref_count > 0)
            meta_ptr->ref_count--;
        if (meta_ptr->ref_count != 0)
            return;

        if (meta_ptr->ptr_table != nullptr) {
            gc_pointer_table::for_each_field(*meta_ptr->ptr_table, ptr,
                                             [this](void **child_ptr) noexcept {
                                                 if (*child_ptr != nullptr)
                                                     decrement_ref_count(*child_ptr);
                                             });
        }

        const auto allocation = allocations_.find(meta_ptr);
        assert(allocation != allocations_.end());

        const size_t released_size = meta_ptr->size;
        free_size_ += released_size;
        block_collected_++;
        allocations_.erase(allocation);
    }

    bool initialized_ = false;
    size_t free_size_ = 0;
    gc_runtime::root_set roots_;
    std::unordered_map<meta_data *, gc_runtime::malloc_ptr<meta_data>> allocations_;
    size_t block_collected_ = 0;
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
    gc_runtime::fatal("Allocation failure");
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

mem_block_info *gc_mem_layout(void) noexcept {
    return nullptr;
}
#endif

} // extern "C"
