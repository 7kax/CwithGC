#include "gc.h"

#include <assert.h>
#include <stdio.h>

void foo(void) {
    const size_t alloc_size = 100;

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

    gc_pop();
}

int main(void) {
    gc_init();

    const size_t alloc_size = 100;

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

    // Record the initial addresses to verify relocation after copying.
    void *pre_ptr = ptr;
    void *pre_ptr2 = ptr2;
    void *pre_ptr3 = ptr3;

    foo(); // block D, E, F allocated here
    assert(gc_root_size() == 3);

    gc_collect();

    // The copying collector relocates objects.
    assert(ptr != pre_ptr);
    assert(ptr2 != pre_ptr2);
    assert(ptr3 != pre_ptr3);

    assert(gc_block_collected() == 3); // Only A, B, and C remain.

    gc_pop();
    assert(gc_root_size() == 0);

    gc_cleanup();

    puts("Copying function call test passed!");

    return 0;
}
