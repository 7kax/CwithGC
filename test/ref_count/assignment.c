#include "../test_debug.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct holder {
    int *child;
};

static gc_ptr_table *create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {offsetof(struct holder, child)};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct holder), 1, pointer_field_offsets);
    TEST_CHECK(table != NULL);
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

    TEST_CHECK(ptr == original);
    TEST_CHECK(*ptr == 42);
    TEST_CHECK(test_gc_free_bytes() == free_bytes);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count);

    gc_pointer_assign(&ptr, NULL);
    gc_scope_end(scope);
}

static void test_child_promotion(gc_ptr_table *table) {
    gc_scope_token scope = gc_scope_begin();
    void *root;
    int *temporary;
    gc_scope_add_root(&root);
    gc_scope_add_root(&temporary);

    gc_pointer_assign(&root, gc_malloc(sizeof(struct holder)));
    gc_register_object(root, table);
    gc_pointer_assign(&temporary, gc_malloc(sizeof(int)));
    *temporary = 43;
    gc_pointer_assign(&((struct holder *)root)->child, temporary);
    gc_pointer_assign(&temporary, NULL);

    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    gc_pointer_assign(&root, ((struct holder *)root)->child);

    TEST_CHECK(*(int *)root == 43);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 1);

    gc_pointer_assign(&root, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 2);
    gc_scope_end(scope);
}

static void test_field_replacement(gc_ptr_table *table) {
    gc_scope_token scope = gc_scope_begin();
    struct holder *parent;
    int *temporary;
    int *replacement;
    gc_scope_add_root(&parent);
    gc_scope_add_root(&temporary);
    gc_scope_add_root(&replacement);

    gc_pointer_assign(&parent, gc_malloc(sizeof(struct holder)));
    gc_register_object(parent, table);
    gc_pointer_assign(&temporary, gc_malloc(sizeof(int)));
    *temporary = 44;
    gc_pointer_assign(&parent->child, temporary);
    gc_pointer_assign(&temporary, NULL);

    gc_pointer_assign(&replacement, gc_malloc(sizeof(int)));
    *replacement = 45;

    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    gc_pointer_assign(&parent->child, replacement);

    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 1);
    TEST_CHECK(parent->child == replacement);
    TEST_CHECK(*parent->child == 45);

    gc_pointer_assign(&replacement, NULL);
    TEST_CHECK(*parent->child == 45);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 1);

    gc_pointer_assign(&parent, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 3);
    gc_scope_end(scope);
}

int main(void) {
    gc_ptr_table *table = create_pointer_table();

    gc_init();

    test_self_assignment();
    test_child_promotion(table);
    test_field_replacement(table);

    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());
    TEST_CHECK(test_gc_root_count() == 0);

    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Reference counting assignment test passed!");
    return 0;
}
