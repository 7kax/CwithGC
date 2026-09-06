#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
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
    assert(table != NULL);
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
    assert(ptr->a == 0); // The entire structure is zero-initialized.
    assert(ptr->b == NULL);
    assert(ptr->c == NULL);
    assert(ptr->d == NULL);
    assert(ptr->e == NULL);
    assert(ptr->f == NULL);
    assert(ptr->g == NULL);
    assert(ptr->h == NULL);
    assert(test_gc_free_bytes() == test_gc_heap_capacity() - struct_block_size);
    assert(test_gc_root_count() == 1);
    assert(test_gc_relocated_block_count() == 0);

    struct pointer_object *pre_ptr = ptr;

    gc_pointer_assign(&ptr->b, gc_malloc(sizeof(int)));
    gc_pointer_assign(&ptr->c, gc_malloc(sizeof(int)));
    gc_pointer_assign(&ptr->d, gc_malloc(sizeof(int)));
    assert(test_gc_relocated_block_count() == 0);
    assert(test_gc_root_count() == 1);

    ptr->a = 666;
    *ptr->b = 42;
    *ptr->c = 43;
    *ptr->d = 44;

    // Trigger garbage collection.
    gc_collect();
    assert(test_gc_relocated_block_count() == 4); // One structure and three integers.

    // The address should change.
    assert(ptr != pre_ptr);

    // The values should remain unchanged.
    assert(ptr->a == 666);
    assert(*ptr->b == 42);
    assert(*ptr->c == 43);
    assert(*ptr->d == 44);

    // Clear the root object and collect.
    gc_pointer_assign((void **)&ptr, NULL);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);

    gc_cleanup();
    gc_ptr_table_destroy(pointer_table);

    puts("Copying recursion test passed!");

    return 0;
}
