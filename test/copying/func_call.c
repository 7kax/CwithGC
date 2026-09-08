#include "../test_debug.h"
#include "../test_types.h"
#include "gc.h"

#include <stdio.h>

void allocate_temporary_objects(void) {
    const size_t alloc_size = 100;
    gc_scope_token scope = gc_scope_begin();

    unsigned char *first_pointer, *second_pointer, *third_pointer;
    gc_scope_add_root(&first_pointer);
    gc_scope_add_root(&second_pointer);
    gc_scope_add_root(&third_pointer);
    TEST_CHECK(first_pointer == NULL);
    TEST_CHECK(second_pointer == NULL);
    TEST_CHECK(third_pointer == NULL);
    gc_pointer_assign(&first_pointer, gc_alloc_array(test_gc_byte_type(), alloc_size));  // block D
    gc_pointer_assign(&second_pointer, gc_alloc_array(test_gc_byte_type(), alloc_size)); // block E
    gc_pointer_assign(&third_pointer, gc_alloc_array(test_gc_byte_type(), alloc_size));  // block F

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 100;

    unsigned char *first_pointer, *second_pointer, *third_pointer;
    gc_scope_add_root(&first_pointer);
    gc_scope_add_root(&second_pointer);
    gc_scope_add_root(&third_pointer);
    TEST_CHECK(first_pointer == NULL);
    TEST_CHECK(second_pointer == NULL);
    TEST_CHECK(third_pointer == NULL);
    gc_pointer_assign(&first_pointer, gc_alloc_array(test_gc_byte_type(), alloc_size));  // block A
    gc_pointer_assign(&second_pointer, gc_alloc_array(test_gc_byte_type(), alloc_size)); // block B
    gc_pointer_assign(&third_pointer, gc_alloc_array(test_gc_byte_type(), alloc_size));  // block C

    allocate_temporary_objects(); // block D, E, F allocated here
    TEST_CHECK(test_gc_root_count() == 3);

    gc_collect();

    TEST_CHECK(test_gc_reclaimed_block_count() == 3); // D, E, and F were unreachable.
    TEST_CHECK(test_gc_relocated_block_count() == 3); // Only A, B, and C remain.

    gc_scope_end(scope);
    TEST_CHECK(test_gc_root_count() == 0);

    gc_cleanup();

    puts("Copying function call test passed!");

    return 0;
}
