#include "../test_layout.h"
#include "../test_types.h"
#include "gc.h"

#include <stdio.h>
#include <stdlib.h>

struct pointer_object {
    int a;
    int *b;
    int *c;
    int *d;
    int *e;
    int *f;
    int *g;
    int *h;
};

static const size_t pointer_object_offsets[] = {
    offsetof(struct pointer_object, b), offsetof(struct pointer_object, c),
    offsetof(struct pointer_object, d), offsetof(struct pointer_object, e),
    offsetof(struct pointer_object, f), offsetof(struct pointer_object, g),
    offsetof(struct pointer_object, h),
};
static const gc_type_descriptor pointer_object_type = {
    sizeof(struct pointer_object),
    sizeof(pointer_object_offsets) / sizeof(pointer_object_offsets[0]),
    pointer_object_offsets,
};

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t struct_block_size = test_gc_block_size(sizeof(struct pointer_object));

    struct pointer_object *ptr;
    gc_scope_add_root(&ptr);
    gc_pointer_assign(&ptr, gc_alloc_object(&pointer_object_type));

    TEST_CHECK(ptr->a == 0);
    TEST_CHECK(ptr->b == NULL);
    TEST_CHECK(ptr->c == NULL);
    TEST_CHECK(ptr->d == NULL);
    TEST_CHECK(ptr->e == NULL);
    TEST_CHECK(ptr->f == NULL);
    TEST_CHECK(ptr->g == NULL);
    TEST_CHECK(ptr->h == NULL);

    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity() - struct_block_size);
    TEST_CHECK(test_gc_root_count() == 1);
    TEST_CHECK(test_gc_reclaimed_block_count() == 0);

    // Allocate and initialize nested references.
    {
        int *temporary = gc_alloc_object(test_gc_int_type());
        gc_pointer_assign(&ptr->b, temporary);
    }
    {
        int *temporary = gc_alloc_object(test_gc_int_type());
        gc_pointer_assign(&ptr->c, temporary);
    }
    {
        int *temporary = gc_alloc_object(test_gc_int_type());
        gc_pointer_assign(&ptr->d, temporary);
    }
    TEST_CHECK(test_gc_reclaimed_block_count() == 0);
    TEST_CHECK(test_gc_root_count() == 1);

    ptr->a = 666;
    *ptr->b = 42;
    *ptr->c = 43;
    *ptr->d = 44;

    // Verify that the values were initialized correctly.
    TEST_CHECK(ptr->a == 666);
    TEST_CHECK(*ptr->b == 42);
    TEST_CHECK(*ptr->c == 43);
    TEST_CHECK(*ptr->d == 44);

    // Clear the references, which should trigger reclamation.
    gc_pointer_assign(&ptr->b, NULL);
    gc_pointer_assign(&ptr->c, NULL);
    gc_pointer_assign(&ptr->d, NULL);

    // Reference counting is immediate, so clearing the references should reclaim them.
    TEST_CHECK(test_gc_reclaimed_block_count() == 3); // Three integer objects.

    // Clear the root object and reclaim it.
    gc_pointer_assign(&ptr, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == 4); // Three integer objects and one structure.

    // All memory should have been reclaimed.
    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);
    gc_cleanup();
    puts("Reference counting recursion test passed!");

    return 0;
}
