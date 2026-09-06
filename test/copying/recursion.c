#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct foo {
    int a;
    int *b;
    int *c;
    int *d;
    int *e;
    int *f;
    int *g;
    int *h;
};

gc_ptr_table *ptr_table = NULL;

gc_ptr_table *construct_ptr_table(void) {
    const size_t positions[] = {
        offsetof(struct foo, b), offsetof(struct foo, c), offsetof(struct foo, d),
        offsetof(struct foo, e), offsetof(struct foo, f), offsetof(struct foo, g),
        offsetof(struct foo, h),
    };
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct foo), 7, positions);
    assert(table != NULL);
    return table;
}

int main(void) {
    ptr_table = construct_ptr_table();

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    const size_t struct_block_size = test_gc_block_size(sizeof(struct foo));

    struct foo *ptr;
    gc_local_var(&ptr);
    gc_ptr_copy(&ptr, gc_malloc(sizeof(struct foo)));
    gc_register(ptr, ptr_table);
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
    assert(test_gc_reclaimed_blocks() == 0);

    struct foo *pre_ptr = ptr;

    gc_ptr_copy(&ptr->b, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->c, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->d, gc_malloc(sizeof(int)));
    assert(test_gc_reclaimed_blocks() == 0);
    assert(test_gc_root_count() == 1);

    ptr->a = 666;
    *ptr->b = 42;
    *ptr->c = 43;
    *ptr->d = 44;

    // Trigger garbage collection.
    gc_collect();
    assert(test_gc_reclaimed_blocks() == 4); // One structure and three integers.

    // The address should change.
    assert(ptr != pre_ptr);

    // The values should remain unchanged.
    assert(ptr->a == 666);
    assert(*ptr->b == 42);
    assert(*ptr->c == 43);
    assert(*ptr->d == 44);

    // Clear the root object and collect.
    gc_ptr_copy((void **)&ptr, NULL);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);

    gc_cleanup();
    gc_ptr_table_destroy(ptr_table);

    puts("Copying recursion test passed!");

    return 0;
}
