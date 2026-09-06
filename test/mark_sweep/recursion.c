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
    const size_t int_block_size = test_gc_block_size(sizeof(int));

    struct foo *ptr;
    gc_local_var(&ptr);
    gc_ptr_copy(&ptr, gc_malloc(sizeof(struct foo)));
    gc_register(ptr, ptr_table);
    assert(ptr->a == 0);
    assert(ptr->b == NULL);
    assert(ptr->c == NULL);
    assert(ptr->d == NULL);
    assert(ptr->e == NULL);
    assert(ptr->f == NULL);
    assert(ptr->g == NULL);
    assert(ptr->h == NULL);
    assert(gc_free_size() == gc_heap_size() - struct_block_size);
    assert(gc_root_size() == 1);
    assert(gc_block_collected() == 0);

    gc_ptr_copy(&ptr->b, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->c, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->d, gc_malloc(sizeof(int)));
    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 1);
    assert(gc_free_size() == gc_heap_size() - struct_block_size - 3 * int_block_size);

    gc_ptr_copy((void **)&ptr, NULL);
    gc_collect();
    assert(gc_block_collected() == 4);
    assert(gc_free_size() == gc_heap_size());

    gc_scope_end(scope);

    gc_cleanup();
    gc_ptr_table_destroy(ptr_table);

    puts("Mark-sweep recursion test passed!");

    return 0;
}
