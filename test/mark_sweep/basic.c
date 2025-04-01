#include "gc.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

int main() {    
    gc_init();
    
    const size_t alloc_size = 1024;
    const size_t meta_size = gc_meta_size();
    const size_t heap_size = gc_heap_size();

    void *ptr, *ptr2;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);

    assert(ptr == NULL);
    assert(ptr2 == NULL);

    gc_ptr_copy(&ptr, gc_malloc(alloc_size)); // block A
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size)); // block B

    mem_block_info *layout = gc_mem_layout();
    assert(layout[0].size == alloc_size + meta_size);
    assert(layout[0].is_free == 0);
    assert(layout[1].size == alloc_size + meta_size);
    assert(layout[1].is_free == 0);
    assert(layout[2].size == heap_size - 2 * (alloc_size + meta_size));
    assert(layout[2].is_free == 1);
    assert(gc_block_collected() == 0);
    free(layout);

    gc_ptr_copy(&ptr, ptr2); // block A is unreachable
    gc_collect();
    
    layout = gc_mem_layout();
    assert(layout[0].size == alloc_size + meta_size);
    assert(layout[0].is_free == 1); // block A is free
    assert(layout[1].size == alloc_size + meta_size);
    assert(layout[1].is_free == 0); // block B is still allocated
    assert(layout[2].size == heap_size - 2 * (alloc_size + meta_size));
    assert(layout[2].is_free == 1);
    assert(gc_block_collected() == 1);
    free(layout);

    void *ptr3;
    gc_local_var(&ptr3);
    assert(ptr3 == NULL);

    gc_ptr_copy(&ptr3, gc_malloc(alloc_size)); // block C

    layout = gc_mem_layout();
    assert(layout[0].size == alloc_size + meta_size);
    assert(layout[0].is_free == 0); // block C allocated here
    assert(layout[1].size == alloc_size + meta_size);
    assert(layout[1].is_free == 0); // block B is still allocated
    assert(layout[2].size == heap_size - 2 * (alloc_size + meta_size));
    assert(layout[2].is_free == 1);
    assert(gc_block_collected() == 1);
    free(layout);

    gc_ptr_copy(&ptr2, ptr3); // no block becomes unreachable
    gc_collect();

    layout = gc_mem_layout();
    assert(layout[0].size == alloc_size + meta_size);
    assert(layout[0].is_free == 0); // block C is still allocated
    assert(layout[1].size == alloc_size + meta_size);
    assert(layout[1].is_free == 0); // block B is still allocated
    assert(layout[2].size == heap_size - 2 * (alloc_size + meta_size));
    assert(layout[2].is_free == 1);
    assert(gc_block_collected() == 1);
    free(layout);

    gc_ptr_copy(&ptr, ptr3); // block B is unreachable
    gc_collect();

    layout = gc_mem_layout();
    assert(layout[0].size == alloc_size + meta_size);
    assert(layout[0].is_free == 0); // block C is still allocated
    assert(layout[1].size == heap_size - (alloc_size + meta_size));
    assert(layout[1].is_free == 1); // block B is collected and merged
    assert(gc_block_collected() == 2);
    free(layout);

    gc_pop();

    gc_cleanup();

    puts("Mark-Sweep basic test passed");

    return 0;
}