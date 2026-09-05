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

    // Release all local variables before returning from the function.
    gc_pop();

    // Reference counting reclaims memory immediately when references are released,
    // so D, E, and F have already been reclaimed.
}

int main(void) {
    gc_init();

    const size_t alloc_size = 100;
    const size_t heap_size = gc_heap_size();

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

    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 3);

    // The initial reclaimed-block count is zero.
    size_t initial_blocks = gc_block_collected();

    foo(); // block D, E, F allocated and freed here

    // Three blocks (D, E, and F) should have been reclaimed.
    assert(gc_block_collected() == initial_blocks + 3);

    // Three roots remain because A, B, and C are still referenced.
    assert(gc_root_size() == 3);

    // Release the local variables in main.
    gc_ptr_copy(&ptr, NULL);
    assert(gc_block_collected() == initial_blocks + 4); // +A

    gc_ptr_copy(&ptr2, NULL);
    assert(gc_block_collected() == initial_blocks + 5); // +B

    gc_ptr_copy(&ptr3, NULL);
    assert(gc_block_collected() == initial_blocks + 6); // +C

    assert(gc_free_size() == heap_size); // All memory should be reclaimed.
    assert(gc_root_size() == 3);         // The number of roots is unchanged.

    gc_pop(); // Remove the roots created in main.
    assert(gc_root_size() == 0);

    gc_cleanup();

    puts("Reference counting function call test passed!");

    return 0;
}
