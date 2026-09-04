#include "../gc_layout.h"
#include "gc.h"
#include <cassert>
#include <cstdlib>
#include <cstring> // 添加 cstring 头文件
#include <iostream>
#include <vector>

void *heap_start;                  // Start of the heap
const size_t heap_size = 4 * 1024; // 4KB

struct stack_ptr {
    void **ptr;  // Local pointer variable
    void *frame; // Address of the frame containing the pointer
};
std::vector<stack_ptr> root; // Stack of pointers as local variables

struct free_block {
    size_t size;      // Size of the free block
    free_block *next; // Pointer to the next free block
};
free_block *free_list; // List of free blocks
size_t free_size;      // Total size of free blocks

struct meta_data {
    u_int8_t marked;         // Marked flag (1 byte)
    gc_ptr_table *ptr_table; // Pointer table
    size_t size;             // Size of the block
};

static_assert(heap_size % gc_layout::alignment == 0);
static_assert(gc_layout::alignment >= alignof(free_block));

constexpr size_t minimum_free_block_size = gc_layout::align_up(sizeof(free_block));

#ifdef GC_DEBUG
size_t block_collected = 0; // Number of blocks collected
#endif

static meta_data *get_meta_data(void *ptr) {
    return gc_layout::metadata<meta_data>(ptr);
}

static void *pick_free_block(size_t size, size_t &allocated_size) {
    assert(free_list != nullptr);
    assert(size > 0);

    free_block *prev = nullptr;
    free_block *cur = free_list;

    while (cur != nullptr && cur->size < size) {
        prev = cur;
        cur = cur->next;
    }

    if (cur == nullptr) {
        // If no free block is large enough, collect garbage
        gc_collect();

        // Try again
        prev = nullptr;
        cur = free_list;
        while (cur != nullptr && cur->size < size) {
            prev = cur;
            cur = cur->next;
        }

        if (cur == nullptr) {
            return nullptr;
        }
    }

    const size_t current_size = cur->size;
    const size_t remainder = current_size - size;

    // Keep the remainder only when it can hold an aligned free-list node.
    if (remainder >= minimum_free_block_size) {
        free_block *new_block = (free_block *)((u_int64_t)cur + size);
        new_block->size = remainder;
        new_block->next = cur->next;

        if (prev == nullptr) {
            free_list = new_block;
        } else {
            prev->next = new_block;
        }

        free_size -= size;
        allocated_size = size;
        return cur;
    } else {
        if (prev == nullptr) {
            free_list = cur->next;
        } else {
            prev->next = cur->next;
        }

        free_size -= current_size;
        allocated_size = current_size;
        return cur;
    }
}

static void mark(void *ptr) {
    meta_data *meta_ptr = get_meta_data(ptr);

    if (meta_ptr->marked)
        return;

    // Mark the block
    meta_ptr->marked = 1;

    if (meta_ptr->ptr_table == nullptr)
        return;

    // Recursively mark the children
    u_int64_t cur_struct = (u_int64_t)ptr;
    for (int i = 0; i < meta_ptr->ptr_table->array_len; i++) {
        for (int j = 0; j < meta_ptr->ptr_table->num_pointers; j++) {
            void **child_ptr = (void **)(cur_struct + meta_ptr->ptr_table->positions[j]);
            if (*child_ptr != nullptr) {
                mark(*child_ptr);
            }
        }
        cur_struct += meta_ptr->ptr_table->struct_size;
    }
}

static void mark_phase() {
    for (auto [ptr, _] : root) {
        if (*ptr != nullptr)
            mark(*ptr);
    }
}

static void sweep_phase() {
    u_int64_t heap_end = (u_int64_t)heap_start + heap_size;

    u_int64_t sweeping = (u_int64_t)heap_start;
    free_block *next_free_block = free_list;
    free_block *prev_free_block = nullptr;

    while (sweeping < heap_end) {
        if (sweeping == (u_int64_t)next_free_block) {
            // Skip the free block
            sweeping += next_free_block->size;

            prev_free_block = next_free_block;
            next_free_block = next_free_block->next;
        } else {
            assert(sweeping < (u_int64_t)next_free_block);
            meta_data *meta_ptr = (meta_data *)sweeping;
            const size_t block_size = meta_ptr->size;

            if (meta_ptr->marked == 0) {
                // Unmarked block
                // Add the block to the free list
                free_block *new_free_block = (free_block *)sweeping;
                new_free_block->size = block_size;
                new_free_block->next = next_free_block;

                if (prev_free_block != nullptr) {
                    prev_free_block->next = new_free_block;
                } else {
                    free_list = new_free_block;
                }
                prev_free_block = new_free_block;

                free_size += block_size;

#ifdef GC_DEBUG
                block_collected++;
#endif
            } else {
                // Marked block
                // Unmark the block
                meta_ptr->marked = 0;
            }

            sweeping += block_size;
        }
    }

    // Merge adjacent free blocks
    // TODO: optimize this, merge in the previous loop
    for (free_block *cur = free_list; cur != nullptr; cur = cur->next) {
        free_block *next = cur->next;
        while (next != nullptr && (char *)cur + cur->size == (char *)next) {
            cur->size += next->size;
            cur->next = next = next->next;
        }
    }
}

extern "C" {
void gc_init() {
    heap_start = std::malloc(heap_size);
    assert(heap_start != nullptr);

    free_list = (free_block *)heap_start;
    free_list->size = heap_size;
    free_list->next = nullptr;

    free_size = heap_size;

#ifdef GC_DEBUG
    block_collected = 0;
#endif
}

void *gc_malloc(size_t size) {
    size_t requested_size;
    if (!gc_layout::block_size<meta_data>(size, requested_size) || requested_size > heap_size)
        gc_allocation_failure();

    size_t allocated_size;
    meta_data *block = (meta_data *)pick_free_block(requested_size, allocated_size);
    if (block == nullptr) {
        gc_allocation_failure();
    }

    block->ptr_table = nullptr;
    block->size = allocated_size;
    block->marked = 0;

    void *payload = gc_layout::payload(block);
    std::memset(payload, 0, size);

    return payload;
}

void gc_local_var(void **ptr) {
    void *frame_address = __builtin_frame_address(1);
    root.push_back({ptr, frame_address});

    // Clear the pointer
    *ptr = nullptr;
}

void gc_register(void *ptr, gc_ptr_table *ptr_map) {
    meta_data *meta_ptr = get_meta_data(ptr);

    // 只允许注册一次
    assert(meta_ptr->ptr_table == nullptr);

    // 保证 ptr_map 合法
    assert(ptr_map != nullptr);
    assert(ptr_map->array_len > 0);
    assert(ptr_map->struct_size > 0);
    assert(ptr_map->num_pointers > 0);

    meta_ptr->ptr_table = ptr_map;
}

void gc_allocation_failure() {
    std::cerr << "Allocation failed\n";
    std::abort();
}

void gc_collect() {
    mark_phase();
    sweep_phase();
}

void gc_cleanup() {
    std::free(heap_start);
    heap_start = nullptr;
    free_list = nullptr;
    free_size = 0;
    root.clear();
}

// No need to handle this in mark-sweep
void gc_ptr_copy(void **dst, void *src) {
    *dst = src;
}

void gc_pop() {
    void *frame_address = root.back().frame;
    while (!root.empty() && root.back().frame == frame_address) {
        root.pop_back();
    }
}

#ifdef GC_DEBUG
size_t gc_heap_size() {
    return heap_size;
}
size_t gc_free_size() {
    return free_size;
}
size_t gc_block_collected() {
    return block_collected;
}
size_t gc_meta_size() {
    return gc_layout::header_size<meta_data>;
}
size_t gc_root_size() {
    return root.size();
}

mem_block_info *gc_mem_layout() {
    std::vector<mem_block_info> mem_blocks;

    void *heap_end = (char *)heap_start + heap_size;

    void *scanning = heap_start;
    free_block *prev_free_block = nullptr;
    free_block *next_free_block = free_list;

    while (scanning < heap_end) {
        if (scanning == next_free_block) {
            size_t size = next_free_block->size;
            void *end = (char *)scanning + size;

            // std::cout << "Start: " << scanning << ", End: " << end
            //           << ", Size: " << size << ", Free\n";
            mem_blocks.push_back({scanning, size, 1});

            scanning = end;

            prev_free_block = next_free_block;
            next_free_block = next_free_block->next;
        } else {
            // size_t size = *(size_t *)((char *)scanning + 1);
            size_t size = ((meta_data *)scanning)->size;
            void *end = (char *)scanning + size;

            // std::cout << "Start: " << scanning << ", End: " << end
            //   << ", Size: " << size << ", Allocated\n";
            mem_blocks.push_back({scanning, size, 0});

            scanning = end;
        }
    }

    mem_block_info *mem_block_array = new mem_block_info[mem_blocks.size() + 1];
    std::copy(mem_blocks.begin(), mem_blocks.end(), mem_block_array);
    mem_block_array[mem_blocks.size()] = {nullptr, 0, 0};

    return mem_block_array;
}
#endif
}
