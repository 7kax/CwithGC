#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
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

    assert(ptr1 == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    gc_pointer_assign(&ptr1, gc_malloc(sizeof(int)));
    gc_pointer_assign(&ptr2, gc_malloc(sizeof(int)));
    gc_pointer_assign(&ptr3, gc_malloc(sizeof(int)));

    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);

    *ptr1 = 42;
    *ptr2 = 43;
    *ptr3 = 44;
    assert(*ptr1 == 42);
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);

    int *pre_ptr1 = ptr1, *pre_ptr2 = ptr2, *pre_ptr3 = ptr3;

    // Collect garbage
    gc_collect();
    assert(test_gc_reclaimed_block_count() == 0);
    assert(test_gc_relocated_block_count() == 3);

    // Address should be different after garbage collection
    assert(ptr1 != pre_ptr1);
    assert(ptr2 != pre_ptr2);
    assert(ptr3 != pre_ptr3);

    // Check if the values are still intact
    assert(*ptr1 == 42);
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);

    // Change reference of ptr1 to NULL
    gc_pointer_assign(&ptr1, NULL);

    // Collect garbage again
    gc_collect();
    assert(test_gc_reclaimed_block_count() == 1);
    assert(test_gc_relocated_block_count() == 5); // 3 + 2
    assert(test_gc_free_bytes() == heap_capacity - 2 * int_block_size);

    // Check if the values are still intact
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);
    assert(ptr1 == NULL);

    gc_scope_end(scope);

    gc_cleanup();

    puts("Copying basic test passed");

    return 0;
}
