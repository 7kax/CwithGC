#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

void allocate_temporary_objects(void) {
    const size_t alloc_size = 100;
    const size_t block_size = test_gc_block_size(alloc_size);
    const size_t heap_capacity = test_gc_heap_capacity();
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

    assert(test_gc_free_bytes() == heap_capacity - 6 * block_size);
    assert(test_gc_reclaimed_block_count() == 0);

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t alloc_size = 100;
    const size_t block_size = test_gc_block_size(alloc_size);
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

    assert(test_gc_free_bytes() == heap_capacity - 3 * block_size);
    assert(test_gc_reclaimed_block_count() == 0);

    allocate_temporary_objects(); // block D, E, F allocated here
    assert(test_gc_root_count() == 3);

    gc_collect();
    assert(test_gc_free_bytes() == heap_capacity - 3 * block_size);
    assert(test_gc_reclaimed_block_count() == 3);

    gc_scope_end(scope);
    assert(test_gc_root_count() == 0);

    gc_cleanup();

    puts("Mark-sweep function call test passed!");

    return 0;
}
