#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    const size_t int_block_size = test_gc_block_size(sizeof(int));
    const size_t heap_size = gc_heap_size();

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    // Allocate three memory blocks.
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

    // Check memory usage.
    assert(gc_free_size() == heap_size - 3 * int_block_size);
    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 3);

    // Clear ptr1, which should trigger reclamation.
    gc_ptr_copy(&ptr1, NULL);
    assert(gc_block_collected() == 1);
    assert(gc_free_size() == heap_size - 2 * int_block_size);

    // Verify that the remaining values are intact.
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);
    assert(ptr1 == NULL);

    // Test shared references.
    int *ptr4;
    gc_local_var(&ptr4);
    gc_ptr_copy(&ptr4, ptr2); // ptr4 and ptr2 share the same object.

    assert(*ptr4 == 43);
    assert(ptr2 == ptr4);

    // The reference count should now be two.
    // Releasing one reference must not reclaim the object.
    gc_ptr_copy(&ptr2, NULL);
    assert(gc_block_collected() == 1); // Still one.
    assert(*ptr4 == 43);

    // Release the final reference; the object should be reclaimed.
    gc_ptr_copy(&ptr4, NULL);
    assert(gc_block_collected() == 2);

    // Release the final object.
    gc_ptr_copy(&ptr3, NULL);
    assert(gc_block_collected() == 3);

    // All memory should have been reclaimed.
    assert(gc_free_size() == heap_size);

    gc_scope_end(scope);
    gc_cleanup();

    puts("Reference counting basic test passed!");

    return 0;
}
