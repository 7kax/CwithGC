#include "../test_layout.h"
#include "gc.h"

#include <stdio.h>

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 1024;
    const size_t block_size = test_gc_block_size(alloc_size);
    const size_t heap_capacity = test_gc_heap_capacity();

    void *ptr, *ptr2;
    gc_scope_add_root(&ptr);
    gc_scope_add_root(&ptr2);

    TEST_CHECK(ptr == NULL);
    TEST_CHECK(ptr2 == NULL);

    gc_pointer_assign(&ptr, gc_malloc(alloc_size));  // block A
    gc_pointer_assign(&ptr2, gc_malloc(alloc_size)); // block B

    gc_debug_memory_layout layout = test_gc_memory_layout();
    TEST_CHECK(layout.block_count == 3);
    TEST_CHECK(layout.blocks[0].size == block_size);
    TEST_CHECK(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED);
    TEST_CHECK(layout.blocks[1].size == block_size);
    TEST_CHECK(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED);
    TEST_CHECK(layout.blocks[2].size == heap_capacity - 2 * block_size);
    TEST_CHECK(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    TEST_CHECK(test_gc_reclaimed_block_count() == 0);
    test_gc_dispose_memory_layout(&layout);

    gc_pointer_assign(&ptr, ptr2); // block A is unreachable
    gc_collect();

    layout = test_gc_memory_layout();
    TEST_CHECK(layout.block_count == 3);
    TEST_CHECK(layout.blocks[0].size == block_size);
    TEST_CHECK(layout.blocks[0].state == GC_DEBUG_BLOCK_FREE); // block A is free
    TEST_CHECK(layout.blocks[1].size == block_size);
    TEST_CHECK(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED); // block B is still allocated
    TEST_CHECK(layout.blocks[2].size == heap_capacity - 2 * block_size);
    TEST_CHECK(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    TEST_CHECK(test_gc_reclaimed_block_count() == 1);
    test_gc_dispose_memory_layout(&layout);

    void *ptr3;
    gc_scope_add_root(&ptr3);
    TEST_CHECK(ptr3 == NULL);

    gc_pointer_assign(&ptr3, gc_malloc(alloc_size)); // block C

    layout = test_gc_memory_layout();
    TEST_CHECK(layout.block_count == 3);
    TEST_CHECK(layout.blocks[0].size == block_size);
    TEST_CHECK(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED); // block C allocated here
    TEST_CHECK(layout.blocks[1].size == block_size);
    TEST_CHECK(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED); // block B is still allocated
    TEST_CHECK(layout.blocks[2].size == heap_capacity - 2 * block_size);
    TEST_CHECK(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    TEST_CHECK(test_gc_reclaimed_block_count() == 1);
    test_gc_dispose_memory_layout(&layout);

    gc_pointer_assign(&ptr2, ptr3); // no block becomes unreachable
    gc_collect();

    layout = test_gc_memory_layout();
    TEST_CHECK(layout.block_count == 3);
    TEST_CHECK(layout.blocks[0].size == block_size);
    TEST_CHECK(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED); // block C is still allocated
    TEST_CHECK(layout.blocks[1].size == block_size);
    TEST_CHECK(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED); // block B is still allocated
    TEST_CHECK(layout.blocks[2].size == heap_capacity - 2 * block_size);
    TEST_CHECK(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    TEST_CHECK(test_gc_reclaimed_block_count() == 1);
    test_gc_dispose_memory_layout(&layout);

    gc_pointer_assign(&ptr, ptr3); // block B is unreachable
    gc_collect();

    layout = test_gc_memory_layout();
    TEST_CHECK(layout.block_count == 2);
    TEST_CHECK(layout.blocks[0].size == block_size);
    TEST_CHECK(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED); // block C is still allocated
    TEST_CHECK(layout.blocks[1].size == heap_capacity - block_size);
    TEST_CHECK(layout.blocks[1].state == GC_DEBUG_BLOCK_FREE); // block B is collected and merged
    TEST_CHECK(test_gc_reclaimed_block_count() == 2);
    test_gc_dispose_memory_layout(&layout);

    gc_scope_end(scope);

    gc_cleanup();

    puts("Mark-Sweep basic test passed");

    return 0;
}
