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
#include <utility>

namespace {

constexpr size_t heap_size = 4 * 1024;

struct meta_data {
    std::uint8_t copied;
    const gc_ptr_table *ptr_table;
    size_t size;
    void *forwarding;
};

static_assert(heap_size % gc_layout::alignment == 0);

class Semispace {
  public:
    void reset(gc_runtime::malloc_ptr<> memory) noexcept {
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

    size_t available() const noexcept { return heap_size - used(); }

    friend void swap(Semispace &lhs, Semispace &rhs) noexcept {
        using std::swap;
        swap(lhs.memory_, rhs.memory_);
        swap(lhs.top_, rhs.top_);
    }

  private:
    gc_runtime::malloc_ptr<> memory_;
    std::byte *top_ = nullptr;
};

class CopyingState {
  public:
    void init() {
        auto new_from = gc_runtime::malloc_bytes(heap_size);
        auto new_to = gc_runtime::malloc_bytes(heap_size);

        roots_.clear();
        from_space_.reset(std::move(new_from));
        to_space_.reset(std::move(new_to));
        block_collected_ = 0;
        initialized_ = true;
    }

    void *allocate(size_t size) {
        require_initialized();

        size_t alloc_size;
        if (!gc_layout::block_size<meta_data>(size, alloc_size) || alloc_size > heap_size)
            gc_allocation_failure();

        if (alloc_size > from_space_.available())
            collect();
        if (alloc_size > from_space_.available())
            gc_allocation_failure();

        auto *block = reinterpret_cast<meta_data *>(from_space_.allocate(alloc_size));

        block->copied = 0;
        block->forwarding = nullptr;
        block->ptr_table = nullptr;
        block->size = alloc_size;

        void *payload = gc_layout::payload(block);
        std::memset(payload, 0, size);
        return payload;
    }

    gc_scope_token begin_scope() {
        require_initialized();

        return roots_.begin_scope();
    }

    void add_root(void *ptr_address) {
        require_initialized();

        roots_.add(ptr_address);
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

        to_space_.rewind();
        std::byte *scan = to_space_.begin();

        roots_.for_each([this](void **ptr_address) noexcept {
            if (*ptr_address != nullptr)
                *ptr_address = evacuate(*ptr_address);
        });

        // Objects copied while scanning are appended to to-space, so the
        // to-space itself acts as the breadth-first work queue.
        while (scan < to_space_.top()) {
            auto *meta_ptr = reinterpret_cast<meta_data *>(scan);
            const size_t block_size = meta_ptr->size;
            scan_object(meta_ptr);
            scan += block_size;
        }

        swap(from_space_, to_space_);
    }

    void end_scope(gc_scope_token token) noexcept {
        require_initialized();
        roots_.end_scope(token);
    }

    void cleanup() noexcept {
        from_space_.clear();
        to_space_.clear();
        block_collected_ = 0;
        roots_.clear();
        initialized_ = false;
    }

    size_t free_size() const noexcept { return initialized_ ? from_space_.available() : 0; }

    size_t block_collected() const noexcept { return block_collected_; }

    size_t root_size() const noexcept { return roots_.size(); }

#ifdef GC_DEBUG
    mem_block_info *memory_layout() const {
        require_initialized();

        gc_layout::layout_builder layout;
        std::byte *current = from_space_.begin();
        while (current < from_space_.top()) {
            auto *meta_ptr = reinterpret_cast<meta_data *>(current);
            layout.add(current, meta_ptr->size, false);
            current += meta_ptr->size;
        }
        layout.add(from_space_.top(), from_space_.available(), true);
        return layout.release();
    }
#endif

  private:
    static meta_data *get_meta_data(void *ptr) noexcept {
        return gc_layout::metadata<meta_data>(ptr);
    }

    void require_initialized() const noexcept { gc_runtime::require_initialized(initialized_); }

    void *evacuate(void *ptr) noexcept {
        meta_data *old_meta = get_meta_data(ptr);
        if (old_meta->copied) {
            assert(old_meta->forwarding != nullptr);
            return old_meta->forwarding;
        }

        const size_t block_size = old_meta->size;
        auto *new_meta = reinterpret_cast<meta_data *>(to_space_.allocate(block_size));
        std::memcpy(new_meta, old_meta, block_size);
        void *new_payload = gc_layout::payload(new_meta);
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

        gc_pointer_table::for_each_field(*ptr_table, gc_layout::payload(meta_ptr),
                                         [this](void **child_ptr) noexcept {
                                             if (*child_ptr != nullptr)
                                                 *child_ptr = evacuate(*child_ptr);
                                         });
    }

    bool initialized_ = false;
    Semispace from_space_;
    Semispace to_space_;
    gc_runtime::root_set roots_;
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

gc_scope_token gc_scope_begin(void) noexcept try { return state.begin_scope(); } catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_scope_end(gc_scope_token token) noexcept try { state.end_scope(token); } catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_local_var(void *ptr_address) noexcept try { state.add_root(ptr_address); } catch (...) {
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
