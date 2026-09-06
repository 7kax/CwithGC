#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 1024;
    const size_t block_size = test_gc_block_size(alloc_size);
    const size_t heap_size = test_gc_heap_capacity();

    void *ptr, *ptr2;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);

    assert(ptr == NULL);
    assert(ptr2 == NULL);

    gc_ptr_copy(&ptr, gc_malloc(alloc_size));  // block A
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size)); // block B

    gc_debug_memory_layout layout = test_gc_memory_layout();
    assert(layout.block_count == 3);
    assert(layout.blocks[0].size == block_size);
    assert(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED);
    assert(layout.blocks[1].size == block_size);
    assert(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED);
    assert(layout.blocks[2].size == heap_size - 2 * block_size);
    assert(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    assert(test_gc_reclaimed_blocks() == 0);
    test_gc_dispose_memory_layout(&layout);

    gc_ptr_copy(&ptr, ptr2); // block A is unreachable
    gc_collect();

    layout = test_gc_memory_layout();
    assert(layout.block_count == 3);
    assert(layout.blocks[0].size == block_size);
    assert(layout.blocks[0].state == GC_DEBUG_BLOCK_FREE); // block A is free
    assert(layout.blocks[1].size == block_size);
    assert(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED); // block B is still allocated
    assert(layout.blocks[2].size == heap_size - 2 * block_size);
    assert(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    assert(test_gc_reclaimed_blocks() == 1);
    test_gc_dispose_memory_layout(&layout);

    void *ptr3;
    gc_local_var(&ptr3);
    assert(ptr3 == NULL);

    gc_ptr_copy(&ptr3, gc_malloc(alloc_size)); // block C

    layout = test_gc_memory_layout();
    assert(layout.block_count == 3);
    assert(layout.blocks[0].size == block_size);
    assert(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED); // block C allocated here
    assert(layout.blocks[1].size == block_size);
    assert(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED); // block B is still allocated
    assert(layout.blocks[2].size == heap_size - 2 * block_size);
    assert(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    assert(test_gc_reclaimed_blocks() == 1);
    test_gc_dispose_memory_layout(&layout);

    gc_ptr_copy(&ptr2, ptr3); // no block becomes unreachable
    gc_collect();

    layout = test_gc_memory_layout();
    assert(layout.block_count == 3);
    assert(layout.blocks[0].size == block_size);
    assert(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED); // block C is still allocated
    assert(layout.blocks[1].size == block_size);
    assert(layout.blocks[1].state == GC_DEBUG_BLOCK_ALLOCATED); // block B is still allocated
    assert(layout.blocks[2].size == heap_size - 2 * block_size);
    assert(layout.blocks[2].state == GC_DEBUG_BLOCK_FREE);
    assert(test_gc_reclaimed_blocks() == 1);
    test_gc_dispose_memory_layout(&layout);

    gc_ptr_copy(&ptr, ptr3); // block B is unreachable
    gc_collect();

    layout = test_gc_memory_layout();
    assert(layout.block_count == 2);
    assert(layout.blocks[0].size == block_size);
    assert(layout.blocks[0].state == GC_DEBUG_BLOCK_ALLOCATED); // block C is still allocated
    assert(layout.blocks[1].size == heap_size - block_size);
    assert(layout.blocks[1].state == GC_DEBUG_BLOCK_FREE); // block B is collected and merged
    assert(test_gc_reclaimed_blocks() == 2);
    test_gc_dispose_memory_layout(&layout);

    gc_scope_end(scope);

    gc_cleanup();

    puts("Mark-Sweep basic test passed");

    return 0;
}
