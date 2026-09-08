#include "../test_layout.h"
#include "../test_types.h"
#include "gc.h"

#include <stdio.h>

int main(void) {
    gc_init();

    const size_t int_block_size = test_gc_block_size(sizeof(int));
    const size_t heap_capacity = test_gc_heap_capacity();

    gc_scope_token scope = gc_scope_begin();

    // Allocate 3 blocks of memory
    int *ptr1, *ptr2, *ptr3;
    gc_scope_add_root(&ptr1);
    gc_scope_add_root(&ptr2);
    gc_scope_add_root(&ptr3);

    TEST_CHECK(ptr1 == NULL);
    TEST_CHECK(ptr2 == NULL);
    TEST_CHECK(ptr3 == NULL);

    gc_pointer_assign(&ptr1, gc_alloc_object(test_gc_int_type()));
    gc_pointer_assign(&ptr2, gc_alloc_object(test_gc_int_type()));
    gc_pointer_assign(&ptr3, gc_alloc_object(test_gc_int_type()));

    TEST_CHECK(ptr1 != NULL);
    TEST_CHECK(ptr2 != NULL);
    TEST_CHECK(ptr3 != NULL);

    *ptr1 = 42;
    *ptr2 = 43;
    *ptr3 = 44;
    TEST_CHECK(*ptr1 == 42);
    TEST_CHECK(*ptr2 == 43);
    TEST_CHECK(*ptr3 == 44);

    // Collect garbage
    gc_collect();
    TEST_CHECK(test_gc_reclaimed_block_count() == 0);
    TEST_CHECK(test_gc_relocated_block_count() == 3);

    // Check if the values are still intact
    TEST_CHECK(*ptr1 == 42);
    TEST_CHECK(*ptr2 == 43);
    TEST_CHECK(*ptr3 == 44);

    // Change reference of ptr1 to NULL
    gc_pointer_assign(&ptr1, NULL);

    // Collect garbage again
    gc_collect();
    TEST_CHECK(test_gc_reclaimed_block_count() == 1);
    TEST_CHECK(test_gc_relocated_block_count() == 5); // 3 + 2
    TEST_CHECK(test_gc_free_bytes() == heap_capacity - 2 * int_block_size);

    // Check if the values are still intact
    TEST_CHECK(*ptr2 == 43);
    TEST_CHECK(*ptr3 == 44);
    TEST_CHECK(ptr1 == NULL);

    gc_scope_end(scope);

    gc_cleanup();

    puts("Copying basic test passed");

    return 0;
}
