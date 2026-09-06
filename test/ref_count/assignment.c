#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct holder {
    int *child;
};

static gc_ptr_table *create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {offsetof(struct holder, child)};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct holder), 1, pointer_field_offsets);
    assert(table != NULL);
    return table;
}

static void test_self_assignment(void) {
    gc_scope_token scope = gc_scope_begin();
    int *ptr;
    gc_scope_add_root(&ptr);
    gc_pointer_assign(&ptr, gc_malloc(sizeof(int)));
    *ptr = 42;

    int *original = ptr;
    const size_t free_bytes = test_gc_free_bytes();
    const size_t reclaimed_count = test_gc_reclaimed_block_count();

    gc_pointer_assign(&ptr, ptr);

    assert(ptr == original);
    assert(*ptr == 42);
    assert(test_gc_free_bytes() == free_bytes);
    assert(test_gc_reclaimed_block_count() == reclaimed_count);

    gc_pointer_assign(&ptr, NULL);
    gc_scope_end(scope);
}

static void test_child_promotion(gc_ptr_table *table) {
    gc_scope_token scope = gc_scope_begin();
    void *root;
    gc_scope_add_root(&root);

    struct holder *parent = gc_malloc(sizeof(struct holder));
    gc_register_object(parent, table);
    gc_pointer_assign(&root, parent);
    gc_pointer_assign(&parent->child, gc_malloc(sizeof(int)));
    *parent->child = 43;

    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    gc_pointer_assign(&root, parent->child);

    assert(*(int *)root == 43);
    assert(test_gc_reclaimed_block_count() == reclaimed_count + 1);

    gc_pointer_assign(&root, NULL);
    assert(test_gc_reclaimed_block_count() == reclaimed_count + 2);
    gc_scope_end(scope);
}

static void test_field_replacement(gc_ptr_table *table) {
    gc_scope_token scope = gc_scope_begin();
    struct holder *parent;
    int *replacement;
    gc_scope_add_root(&parent);
    gc_scope_add_root(&replacement);

    gc_pointer_assign(&parent, gc_malloc(sizeof(struct holder)));
    gc_register_object(parent, table);
    gc_pointer_assign(&parent->child, gc_malloc(sizeof(int)));
    *parent->child = 44;

    gc_pointer_assign(&replacement, gc_malloc(sizeof(int)));
    *replacement = 45;

    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    gc_pointer_assign(&parent->child, replacement);

    assert(test_gc_reclaimed_block_count() == reclaimed_count + 1);
    assert(parent->child == replacement);
    assert(*parent->child == 45);

    gc_pointer_assign(&replacement, NULL);
    assert(*parent->child == 45);
    assert(test_gc_reclaimed_block_count() == reclaimed_count + 1);

    gc_pointer_assign(&parent, NULL);
    assert(test_gc_reclaimed_block_count() == reclaimed_count + 3);
    gc_scope_end(scope);
}

int main(void) {
    gc_ptr_table *table = create_pointer_table();

    gc_init();

    test_self_assignment();
    test_child_promotion(table);
    test_field_replacement(table);

    assert(test_gc_free_bytes() == test_gc_heap_capacity());
    assert(test_gc_root_count() == 0);

    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Reference counting assignment test passed!");
    return 0;
}
