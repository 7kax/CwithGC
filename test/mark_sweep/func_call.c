#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

void foo(void) {
    const size_t alloc_size = 100;
    const size_t block_size = test_gc_block_size(alloc_size);
    const size_t heap_size = test_gc_heap_capacity();
    gc_scope_token scope = gc_scope_begin();

    void *ptr, *ptr2, *ptr3;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);
    assert(ptr == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);
    gc_ptr_copy(&ptr, gc_malloc(alloc_size));  // block D
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size)); // block E
    gc_ptr_copy(&ptr3, gc_malloc(alloc_size)); // block F

    assert(test_gc_free_bytes() == heap_size - 6 * block_size);
    assert(test_gc_reclaimed_blocks() == 0);

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 100;
    const size_t block_size = test_gc_block_size(alloc_size);
    const size_t heap_size = test_gc_heap_capacity();

    void *ptr, *ptr2, *ptr3;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);
    assert(ptr == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);
    gc_ptr_copy(&ptr, gc_malloc(alloc_size));  // block A
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size)); // block B
    gc_ptr_copy(&ptr3, gc_malloc(alloc_size)); // block C

    assert(test_gc_free_bytes() == heap_size - 3 * block_size);
    assert(test_gc_reclaimed_blocks() == 0);

    foo(); // block D, E, F allocated here
    assert(test_gc_root_count() == 3);

    gc_collect();
    assert(test_gc_free_bytes() == heap_size - 3 * block_size);
    assert(test_gc_reclaimed_blocks() == 3);

    gc_scope_end(scope);
    assert(test_gc_root_count() == 0);

    gc_cleanup();

    puts("Mark-sweep function call test passed!");

    return 0;
}
