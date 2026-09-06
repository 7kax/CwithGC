#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

void foo(void) {
    const size_t alloc_size = 100;
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

    // Release all local variables before returning from the function.
    gc_scope_end(scope);

    // Reference counting reclaims memory immediately when references are released,
    // so D, E, and F have already been reclaimed.
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 100;
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

    assert(test_gc_reclaimed_blocks() == 0);
    assert(test_gc_root_count() == 3);

    // The initial reclaimed-block count is zero.
    size_t initial_blocks = test_gc_reclaimed_blocks();

    foo(); // block D, E, F allocated and freed here

    // Three blocks (D, E, and F) should have been reclaimed.
    assert(test_gc_reclaimed_blocks() == initial_blocks + 3);

    // Three roots remain because A, B, and C are still referenced.
    assert(test_gc_root_count() == 3);

    // Release the local variables in main.
    gc_ptr_copy(&ptr, NULL);
    assert(test_gc_reclaimed_blocks() == initial_blocks + 4); // +A

    gc_ptr_copy(&ptr2, NULL);
    assert(test_gc_reclaimed_blocks() == initial_blocks + 5); // +B

    gc_ptr_copy(&ptr3, NULL);
    assert(test_gc_reclaimed_blocks() == initial_blocks + 6); // +C

    assert(test_gc_free_bytes() == heap_size); // All memory should be reclaimed.
    assert(test_gc_root_count() == 3);         // The number of roots is unchanged.

    gc_scope_end(scope); // Remove the roots created in main.
    assert(test_gc_root_count() == 0);

    gc_cleanup();

    puts("Reference counting function call test passed!");

    return 0;
}
