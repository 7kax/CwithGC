#include "gc.h"
#include <iostream>
#include <cstdlib>
#include <cassert>
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
    void *ptr_table; // Pointer to the table of pointers, lowest bits are used
                     // as flags
    size_t size;     // Size of the block
};
const size_t reserved_bits = 3; // Number of bits reserved for flags
const u_int64_t reserved_mask = ((1 << reserved_bits) - 1);
const u_int64_t mark_mask = 0x01;
const size_t small_block_threshold = (64 - reserved_bits) * sizeof(void *);

#ifdef GC_DEBUG
size_t block_collected = 0; // Number of blocks collected
#endif

static void *pick_free_block(size_t size) {
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

    // Split the block if it is larger than the requested size
    if (cur->size > size + sizeof(free_block)) {
        free_block *new_block = (free_block *)((char *)cur + size);
        new_block->size = cur->size - size;
        new_block->next = cur->next;

        if (prev == nullptr) {
            free_list = new_block;
        } else {
            prev->next = new_block;
        }

        free_size -= size;
        return cur;
    } else {
        if (prev == nullptr) {
            free_list = cur->next;
        } else {
            prev->next = cur->next;
        }

        free_size -= cur->size;
        return cur;
    }
}

static void mark(void *ptr) {
    meta_data *meta_ptr = (meta_data *)((u_int64_t)ptr - sizeof(meta_data));
    u_int64_t marked = (u_int64_t)meta_ptr->ptr_table & mark_mask;

    if (marked) {
        return;
    }

    meta_ptr->ptr_table = (void *)((u_int64_t)meta_ptr->ptr_table | mark_mask);
    // Recursively mark the children
    size_t size = meta_ptr->size - sizeof(meta_data);
    u_int64_t *table_ptr =
        (size <= small_block_threshold)
            ? (u_int64_t *)meta_ptr
            : (u_int64_t *)((u_int64_t)meta_ptr->ptr_table & ~reserved_mask);

    if (table_ptr == nullptr) {
        return;
    }

    for (int i = 0; i < size / sizeof(u_int64_t); i++) {
        int idx = i / 64;
        int byte_pos = 7 - (i % 64) / 8;
        int bit_pos = 7 - (i % 64) % 8;
        u_int64_t mask = (u_int64_t)(1) << (byte_pos * 8 + bit_pos);
        if (table_ptr[idx] & mask) {
            // The pointer is marked, so we need to mark the object it points to
            void **child_ptr = (void **)((char *)ptr + i * sizeof(u_int64_t));
            if (*child_ptr != nullptr) {
                mark(*child_ptr);
            }
        }
    }
}

static void mark_phase() {
    for (auto [ptr, _] : root) {
        if (*ptr != nullptr)
            mark(*ptr);
    }
}

static void sweep_phase() {
    void *heap_end = (char *)heap_start + heap_size;

    void *sweeping = heap_start;
    free_block *next_free_block = free_list;
    free_block *prev_free_block = nullptr;

    while (sweeping < heap_end) {
        if (sweeping == next_free_block) {
            // Skip the free block
            sweeping = (char *)sweeping + next_free_block->size;

            prev_free_block = next_free_block;
            next_free_block = next_free_block->next;
        } else {
            assert(sweeping < next_free_block);
            // char *mark_byte = (char *)sweeping;
            u_int64_t marked =
                (u_int64_t)((meta_data *)sweeping)->ptr_table & mark_mask;
            // size_t size = *(size_t *)(mark_byte + 1);
            size_t size = ((meta_data *)sweeping)->size;

            if (marked == 0) {
                // Unmarked block
                // Add the block to the free list
                free_block *new_free_block = (free_block *)sweeping;
                new_free_block->size = size;
                new_free_block->next = next_free_block;

                if (prev_free_block != nullptr) {
                    prev_free_block->next = new_free_block;
                } else {
                    free_list = new_free_block;
                }
                prev_free_block = new_free_block;

                free_size += size;

#ifdef GC_DEBUG
                block_collected++;
#endif
            } else {
                // Marked block
                // *mark_byte = 0; // Unmark the block
                ((meta_data *)sweeping)->ptr_table =
                    (void *)((u_int64_t)((meta_data *)sweeping)->ptr_table &
                             ~mark_mask);
            }

            sweeping = (char *)sweeping + size;
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

/*
Structure of the allocated block:
mark (1 byte)
size (size_t)
data (size bytes)

Structure of the free block:
size (size_t)
next (free_block *)
(padding)
*/
void *gc_malloc(size_t size) {
    size_t alloc_size = size + sizeof(meta_data);
    void *block = pick_free_block(alloc_size);
    if (block == nullptr) {
        gc_allocation_failure();
    }

    ((meta_data *)block)->ptr_table = nullptr;
    ((meta_data *)block)->size = alloc_size;

    return ((char *)block + sizeof(meta_data)); 
}

void gc_local_var(void **ptr) {
    void *frame_address = __builtin_frame_address(1);
    root.push_back({ptr, frame_address});

    // Clear the pointer
    *ptr = nullptr;
}

/*
Small block:
ptr_table | flags (8 bytes)
size (size_t)
...

Large block:
ptr_table_address | flags (8 bytes)
size (size_t)
...
*/
void gc_register(void *ptr, void *ptr_map) {
    meta_data *meta_ptr = (meta_data *)((u_int64_t)ptr - sizeof(meta_data));
    size_t data_size = meta_ptr->size - sizeof(meta_data);

    u_int64_t flags = (u_int64_t)meta_ptr->ptr_table & reserved_mask;
    if (data_size <= small_block_threshold) {
        u_int64_t ptr_table = (*((u_int64_t *)ptr_map)) & ~reserved_mask;
        meta_ptr->ptr_table = (void *)(ptr_table | flags);
    } else {
        u_int64_t ptr_table = (u_int64_t)ptr_map & ~reserved_mask;
        meta_ptr->ptr_table = (void *)(ptr_table | flags);
    }

    // Set child pointers to null
    size_t size = meta_ptr->size - sizeof(meta_data);
    u_int64_t *table_ptr =
        (size <= small_block_threshold)
            ? (u_int64_t *)meta_ptr
            : (u_int64_t *)((u_int64_t)meta_ptr->ptr_table & ~reserved_mask);
    for (int i = 0; i < size / sizeof(u_int64_t); i++) {
        int idx = i / 64;
        int byte_pos = 7 - (i % 64) / 8;
        int bit_pos = 7 - (i % 64) % 8;
        u_int64_t mask = (u_int64_t)(1) << (byte_pos * 8 + bit_pos);
        if (table_ptr[idx] & mask) {
            // The pointer is marked, so we need to set it to null
            void **child_ptr = (void **)((char *)ptr + i * sizeof(u_int64_t));
            *child_ptr = nullptr;
        }
    }
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
void gc_ptr_copy(void **dst, void *src) { *dst = src; }

void gc_pop() {
    void *frame_address = root.back().frame;
    while (!root.empty() && root.back().frame == frame_address) {
        root.pop_back();
    }
}

#ifdef GC_DEBUG
size_t gc_heap_size() { return heap_size; }
size_t gc_free_size() { return free_size; }
size_t gc_block_collected() { return block_collected; }
size_t gc_meta_size() { return sizeof(meta_data); }
size_t gc_root_size() { return root.size(); }

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

    mem_block_info *mem_block_array = (mem_block_info *)std::malloc(
        (mem_blocks.size() + 1) * sizeof(mem_block_info));
    std::copy(mem_blocks.begin(), mem_blocks.end(), mem_block_array);
    mem_block_array[mem_blocks.size()] = {nullptr, 0, 0};

    return mem_block_array;
}
#endif
}
