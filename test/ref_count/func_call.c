#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

void allocate_temporary_objects(void) {
    const size_t alloc_size = 100;
    gc_scope_token scope = gc_scope_begin();

    void *first_pointer, *second_pointer, *third_pointer;
    gc_scope_add_root(&first_pointer);
    gc_scope_add_root(&second_pointer);
    gc_scope_add_root(&third_pointer);

    assert(first_pointer == NULL);
    assert(second_pointer == NULL);
    assert(third_pointer == NULL);

    gc_pointer_assign(&first_pointer, gc_malloc(alloc_size));  // block D
    gc_pointer_assign(&second_pointer, gc_malloc(alloc_size)); // block E
    gc_pointer_assign(&third_pointer, gc_malloc(alloc_size));  // block F

    // Release all local variables before returning from the function.
    gc_scope_end(scope);

    // Reference counting reclaims memory immediately when references are released,
    // so D, E, and F have already been reclaimed.
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 100;
    const size_t heap_capacity = test_gc_heap_capacity();

    void *first_pointer, *second_pointer, *third_pointer;
    gc_scope_add_root(&first_pointer);
    gc_scope_add_root(&second_pointer);
    gc_scope_add_root(&third_pointer);

    assert(first_pointer == NULL);
    assert(second_pointer == NULL);
    assert(third_pointer == NULL);

    gc_pointer_assign(&first_pointer, gc_malloc(alloc_size));  // block A
    gc_pointer_assign(&second_pointer, gc_malloc(alloc_size)); // block B
    gc_pointer_assign(&third_pointer, gc_malloc(alloc_size));  // block C

    assert(test_gc_reclaimed_block_count() == 0);
    assert(test_gc_root_count() == 3);

    // The initial reclaimed-block count is zero.
    size_t initial_reclaimed_count = test_gc_reclaimed_block_count();

    allocate_temporary_objects(); // block D, E, F allocated and freed here

    // Three blocks (D, E, and F) should have been reclaimed.
    assert(test_gc_reclaimed_block_count() == initial_reclaimed_count + 3);

    // Three roots remain because A, B, and C are still referenced.
    assert(test_gc_root_count() == 3);

    // Release the local variables in main.
    gc_pointer_assign(&first_pointer, NULL);
    assert(test_gc_reclaimed_block_count() == initial_reclaimed_count + 4); // +A

    gc_pointer_assign(&second_pointer, NULL);
    assert(test_gc_reclaimed_block_count() == initial_reclaimed_count + 5); // +B

    gc_pointer_assign(&third_pointer, NULL);
    assert(test_gc_reclaimed_block_count() == initial_reclaimed_count + 6); // +C

    assert(test_gc_free_bytes() == heap_capacity); // All memory should be reclaimed.
    assert(test_gc_root_count() == 3);             // The number of roots is unchanged.

    gc_scope_end(scope); // Remove the roots created in main.
    assert(test_gc_root_count() == 0);

    gc_cleanup();

    puts("Reference counting function call test passed!");

    return 0;
}
