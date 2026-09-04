#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

int main() {
    const size_t int_block_size = test_gc_block_size(sizeof(int));
    const size_t heap_size = gc_heap_size();

    gc_init();

    // Allocate 3 blocks of memory
    int *ptr1, *ptr2, *ptr3;
    gc_local_var(&ptr1);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);

    assert(ptr1 == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    gc_ptr_copy(&ptr1, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr2, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr3, gc_malloc(sizeof(int)));

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
    assert(gc_block_collected() == 3);

    // Address should be different after garbage collection
    assert(ptr1 != pre_ptr1);
    assert(ptr2 != pre_ptr2);
    assert(ptr3 != pre_ptr3);

    // Check if the values are still intact
    assert(*ptr1 == 42);
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);

    // Change reference of ptr1 to NULL
    gc_ptr_copy(&ptr1, NULL);

    // Collect garbage again
    gc_collect();
    assert(gc_block_collected() == 5); // 3 + 2
    assert(gc_free_size() == heap_size - 2 * int_block_size);

    // Check if the values are still intact
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);
    assert(ptr1 == NULL);

    gc_pop();

    gc_cleanup();

    puts("Copying basic test passed");

    return 0;
}
