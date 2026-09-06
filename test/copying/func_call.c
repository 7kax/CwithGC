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

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 100;

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

    // Record the initial addresses to verify relocation after copying.
    void *pre_first_pointer = first_pointer;
    void *pre_second_pointer = second_pointer;
    void *pre_third_pointer = third_pointer;

    allocate_temporary_objects(); // block D, E, F allocated here
    assert(test_gc_root_count() == 3);

    gc_collect();

    // The copying collector relocates objects.
    assert(first_pointer != pre_first_pointer);
    assert(second_pointer != pre_second_pointer);
    assert(third_pointer != pre_third_pointer);

    assert(test_gc_reclaimed_block_count() == 3); // D, E, and F were unreachable.
    assert(test_gc_relocated_block_count() == 3); // Only A, B, and C remain.

    gc_scope_end(scope);
    assert(test_gc_root_count() == 0);

    gc_cleanup();

    puts("Copying function call test passed!");

    return 0;
}
