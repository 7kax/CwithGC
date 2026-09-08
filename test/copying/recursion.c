#include "../test_layout.h"
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

gc_ptr_table *pointer_table = NULL;

gc_ptr_table *create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct pointer_object, b), offsetof(struct pointer_object, c),
        offsetof(struct pointer_object, d), offsetof(struct pointer_object, e),
        offsetof(struct pointer_object, f), offsetof(struct pointer_object, g),
        offsetof(struct pointer_object, h),
    };
    gc_ptr_table *table =
        gc_ptr_table_create(1, sizeof(struct pointer_object), 7, pointer_field_offsets);
    TEST_CHECK(table != NULL);
    return table;
}

int main(void) {
    pointer_table = create_pointer_table();

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t struct_block_size = test_gc_block_size(sizeof(struct pointer_object));

    struct pointer_object *ptr;
    gc_scope_add_root(&ptr);
    gc_pointer_assign(&ptr, gc_malloc(sizeof(struct pointer_object)));
    gc_register_object(ptr, pointer_table);
    TEST_CHECK(ptr->a == 0); // The entire structure is zero-initialized.
    TEST_CHECK(ptr->b == NULL);
    TEST_CHECK(ptr->c == NULL);
    TEST_CHECK(ptr->d == NULL);
    TEST_CHECK(ptr->e == NULL);
    TEST_CHECK(ptr->f == NULL);
    TEST_CHECK(ptr->g == NULL);
    TEST_CHECK(ptr->h == NULL);
    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity() - struct_block_size);
    TEST_CHECK(test_gc_root_count() == 1);
    TEST_CHECK(test_gc_relocated_block_count() == 0);

    {
        int *temporary = gc_malloc(sizeof(int));
        gc_pointer_assign(&ptr->b, temporary);
    }
    {
        int *temporary = gc_malloc(sizeof(int));
        gc_pointer_assign(&ptr->c, temporary);
    }
    {
        int *temporary = gc_malloc(sizeof(int));
        gc_pointer_assign(&ptr->d, temporary);
    }
    TEST_CHECK(test_gc_relocated_block_count() == 0);
    TEST_CHECK(test_gc_root_count() == 1);

    ptr->a = 666;
    *ptr->b = 42;
    *ptr->c = 43;
    *ptr->d = 44;

    // Trigger garbage collection.
    gc_collect();
    TEST_CHECK(test_gc_relocated_block_count() == 4); // One structure and three integers.

    // The values should remain unchanged.
    TEST_CHECK(ptr->a == 666);
    TEST_CHECK(*ptr->b == 42);
    TEST_CHECK(*ptr->c == 43);
    TEST_CHECK(*ptr->d == 44);

    // Clear the root object and collect.
    gc_pointer_assign(&ptr, NULL);
    gc_collect();
    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);

    gc_cleanup();
    gc_ptr_table_destroy(pointer_table);

    puts("Copying recursion test passed!");

    return 0;
}
