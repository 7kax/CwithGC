#include "../test_layout.h"
#include "gc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct holder {
    int *child;
};

static gc_ptr_table *construct_ptr_table(void) {
    const size_t positions[] = {offsetof(struct holder, child)};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct holder), 1, positions);
    assert(table != NULL);
    return table;
}

static void test_self_assignment(void) {
    int *ptr;
    gc_local_var(&ptr);
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int)));
    *ptr = 42;

    int *original = ptr;
    const size_t free_size = gc_free_size();
    const size_t collected = gc_block_collected();

    gc_ptr_copy(&ptr, ptr);

    assert(ptr == original);
    assert(*ptr == 42);
    assert(gc_free_size() == free_size);
    assert(gc_block_collected() == collected);

    gc_ptr_copy(&ptr, NULL);
    gc_pop();
}

static void test_child_promotion(gc_ptr_table *table) {
    void *root;
    gc_local_var(&root);

    struct holder *parent = gc_malloc(sizeof(struct holder));
    gc_register(parent, table);
    gc_ptr_copy(&root, parent);
    gc_ptr_copy(&parent->child, gc_malloc(sizeof(int)));
    *parent->child = 43;

    const size_t collected = gc_block_collected();
    gc_ptr_copy(&root, parent->child);

    assert(*(int *)root == 43);
    assert(gc_block_collected() == collected + 1);

    gc_ptr_copy(&root, NULL);
    assert(gc_block_collected() == collected + 2);
    gc_pop();
}

static void test_field_replacement(gc_ptr_table *table) {
    struct holder *parent;
    int *replacement;
    gc_local_var(&parent);
    gc_local_var(&replacement);

    gc_ptr_copy(&parent, gc_malloc(sizeof(struct holder)));
    gc_register(parent, table);
    gc_ptr_copy(&parent->child, gc_malloc(sizeof(int)));
    *parent->child = 44;

    gc_ptr_copy(&replacement, gc_malloc(sizeof(int)));
    *replacement = 45;

    const size_t collected = gc_block_collected();
    gc_ptr_copy(&parent->child, replacement);

    assert(gc_block_collected() == collected + 1);
    assert(parent->child == replacement);
    assert(*parent->child == 45);

    gc_ptr_copy(&replacement, NULL);
    assert(*parent->child == 45);
    assert(gc_block_collected() == collected + 1);

    gc_ptr_copy(&parent, NULL);
    assert(gc_block_collected() == collected + 3);
    gc_pop();
}

int main(void) {
    gc_ptr_table *table = construct_ptr_table();

    gc_init();

    test_self_assignment();
    test_child_promotion(table);
    test_field_replacement(table);

    assert(gc_free_size() == gc_heap_size());
    assert(gc_root_size() == 0);

    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Reference counting assignment test passed!");
    return 0;
}
