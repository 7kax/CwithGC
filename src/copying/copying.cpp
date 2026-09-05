#include "../gc_layout.h"
#include "../gc_ptr_table_internal.h"
#include "../gc_runtime.h"
#include "gc.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

void *from;                        // Start of the from space
void *to;                          // Start of the to space
const size_t heap_size = 4 * 1024; // 4KB

void *free_space; // Start of the free space in from space
size_t free_size; // Size of the free space in from space

struct stack_ptr {
    void **ptr;
    void *frame;
};
std::vector<stack_ptr> root;

struct meta_data {
    u_int8_t copied;
    const gc_ptr_table *ptr_table;
    size_t size;
    void *forwarding;
};

static_assert(heap_size % gc_layout::alignment == 0);

#ifdef GC_DEBUG
size_t block_collected = 0;
#endif

static meta_data *get_meta_data(void *ptr) {
    return gc_layout::metadata<meta_data>(ptr);
}

[[noreturn]] static void invalid_pointer_table() {
    gc_runtime::fatal("Invalid pointer table");
}

static void *evacuate(void *ptr) {
    meta_data *old_meta = get_meta_data(ptr);

    if (old_meta->copied) {
        assert(old_meta->forwarding != nullptr);
        return old_meta->forwarding;
    }

    // Copy the block and reserve its space before evacuating more objects.
    size_t block_size = old_meta->size;
    meta_data *new_meta = static_cast<meta_data *>(free_space);
    std::memcpy(new_meta, old_meta, block_size);
    void *new_payload = gc_layout::payload(new_meta);
    free_space = reinterpret_cast<char *>(free_space) + block_size;

#ifdef GC_DEBUG
    block_collected++;
#endif

    // Publish the forwarding address before scanning so aliases and cycles are
    // handled without copying the object again.
    old_meta->copied = 1;
    old_meta->forwarding = new_payload;

    // The destination object is an unforwarded object in the next collection.
    new_meta->copied = 0;
    new_meta->forwarding = nullptr;

    return new_payload;
}

static void scan_object(meta_data *meta_ptr) {
    const gc_ptr_table *ptr_table = meta_ptr->ptr_table;
    if (ptr_table == nullptr)
        return;

    char *cur_struct = static_cast<char *>(gc_layout::payload(meta_ptr));
    for (size_t i = 0; i < ptr_table->array_len; i++) {
        for (size_t j = 0; j < ptr_table->positions.size(); j++) {
            void **child_ptr = reinterpret_cast<void **>(cur_struct + ptr_table->positions[j]);
            if (*child_ptr != nullptr)
                *child_ptr = evacuate(*child_ptr);
        }
        cur_struct += ptr_table->struct_size;
    }
}

extern "C" {
void gc_init(void) noexcept try {
    from = std::malloc(heap_size);
    to = std::malloc(heap_size);
    free_space = from;
    free_size = heap_size;

#ifdef GC_DEBUG
    block_collected = 0;
#endif
} catch (...) {
    gc_runtime::handle_current_exception();
}

void *gc_malloc(size_t size) noexcept try {
    size_t alloc_size;
    if (!gc_layout::block_size<meta_data>(size, alloc_size) || alloc_size > heap_size)
        gc_allocation_failure();

    // If the allocation size is larger than the free size, collect garbage
    if (alloc_size > free_size)
        gc_collect();

    // If the allocation size is still larger than the free size, fail
    if (alloc_size > free_size)
        gc_allocation_failure();

    // Allocate the block
    meta_data *block = (meta_data *)free_space;
    free_space = (char *)free_space + alloc_size;
    free_size -= alloc_size;

    // Set the meta data
    block->copied = 0;
    block->forwarding = nullptr;
    block->ptr_table = nullptr;
    block->size = alloc_size;

    // Clear the block
    void *payload = gc_layout::payload(block);
    std::memset(payload, 0, size);

    return payload;
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_local_var(void *ptr_address) noexcept try {
    auto **ptr = static_cast<void **>(ptr_address);
    void *frame_address = __builtin_frame_address(1);
    root.push_back({ptr, frame_address});

    *ptr = nullptr; // Clear the pointer
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_register(void *ptr, const gc_ptr_table *ptr_map) noexcept try {
    meta_data *meta_ptr = get_meta_data(ptr);

    // 只允许注册一次
    assert(meta_ptr->ptr_table == nullptr);

    // 保证 ptr_map 合法
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
} catch (...) {
    gc_runtime::handle_current_exception();
}

// No need to handle this in copying
void gc_ptr_copy(void *dst_address, void *src) noexcept {
    auto **dst = static_cast<void **>(dst_address);
    *dst = src;
}

void gc_collect(void) noexcept try {
    free_space = to;
    char *scan = static_cast<char *>(to);

    for (auto [ptr_address, frame] : root)
        if (*ptr_address != nullptr)
            *ptr_address = evacuate(*ptr_address);

    // Objects copied while scanning are appended at free_space, so the to-space
    // itself acts as the breadth-first work queue.
    while (scan < static_cast<char *>(free_space)) {
        meta_data *meta_ptr = reinterpret_cast<meta_data *>(scan);
        size_t block_size = meta_ptr->size;
        scan_object(meta_ptr);
        scan += block_size;
    }

    // Swap the spaces
    std::swap(from, to);

    // Reset free size
    free_size = heap_size - ((char *)free_space - (char *)from);
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_pop(void) noexcept {
    void *frame_address = root.back().frame;
    while (!root.empty() && root.back().frame == frame_address) {
        root.pop_back();
    }
}

void gc_cleanup(void) noexcept {
    std::free(from);
    std::free(to);
    from = nullptr;
    to = nullptr;
    free_space = nullptr;
    free_size = 0;
    root.clear();
}

void gc_allocation_failure(void) noexcept {
    gc_runtime::fatal("Allocation failure");
}

#ifdef GC_DEBUG
size_t gc_heap_size(void) noexcept {
    return heap_size;
}
size_t gc_free_size(void) noexcept {
    return free_size;
}
size_t gc_block_collected(void) noexcept {
    return block_collected;
}
size_t gc_meta_size(void) noexcept {
    return gc_layout::header_size<meta_data>;
}
size_t gc_root_size(void) noexcept {
    return root.size();
}
mem_block_info *gc_mem_layout(void) noexcept try {
    std::vector<mem_block_info> mem_layout;

    void *current = from;
    while (current < free_space) {
        mem_block_info block;
        block.start = current;
        block.size = ((meta_data *)current)->size;
        block.is_free = 0;
        mem_layout.push_back(block);
        current = (char *)current + block.size;
    }

    mem_layout.push_back({free_space, free_size, 1});

    mem_block_info *layout = new mem_block_info[mem_layout.size() + 1];
    std::copy(mem_layout.begin(), mem_layout.end(), layout);
    layout[mem_layout.size()] = {nullptr, 0, 0}; // Null-terminate the array

    return layout;
} catch (...) {
    gc_runtime::handle_current_exception();
}
#endif
}
