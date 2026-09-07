#include "../test_check.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>

struct graph_node {
    int value;
    struct graph_node *left;
    struct graph_node *right;
};

static gc_ptr_table *create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct graph_node, left),
        offsetof(struct graph_node, right),
    };
    gc_ptr_table *table =
        gc_ptr_table_create(1, sizeof(struct graph_node), 2, pointer_field_offsets);
    TEST_CHECK(table != NULL);
    return table;
}

static void allocate_node(struct graph_node **slot, int value, gc_ptr_table *table) {
    gc_pointer_assign(slot, gc_malloc(sizeof(struct graph_node)));
    TEST_CHECK(*slot != NULL);
    gc_register_object(*slot, table);
    (*slot)->value = value;
}

static void check_graph(const struct graph_node *root, const struct graph_node *shared) {
    TEST_CHECK(root != NULL);
    TEST_CHECK(shared != NULL);
    TEST_CHECK(root->value == 1);
    TEST_CHECK(root->left != NULL);
    TEST_CHECK(root->right != NULL);
    TEST_CHECK(root->left->value == 2);
    TEST_CHECK(root->right->value == 3);
    TEST_CHECK(root->left->left != NULL);
    TEST_CHECK(root->left->left == root->right->right);
    TEST_CHECK(root->left->left == shared);
    TEST_CHECK(root->left->left->value == 4);
    TEST_CHECK(root->left->right == NULL);
    TEST_CHECK(root->right->left == NULL);
    TEST_CHECK(root->right->right->left == NULL);
    TEST_CHECK(root->right->right->right == NULL);
}

int main(void) {
    gc_ptr_table *table = create_pointer_table();

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct graph_node *root;
    struct graph_node *left;
    struct graph_node *right;
    struct graph_node *leaf;
    struct graph_node *shared;
    gc_scope_add_root(&root);
    gc_scope_add_root(&left);
    gc_scope_add_root(&right);
    gc_scope_add_root(&leaf);
    gc_scope_add_root(&shared);

    allocate_node(&root, 1, table);
    allocate_node(&left, 2, table);
    allocate_node(&right, 3, table);
    allocate_node(&leaf, 4, table);

    gc_pointer_assign(&root->left, left);
    gc_pointer_assign(&root->right, right);
    gc_pointer_assign(&left->left, leaf);
    gc_pointer_assign(&right->right, leaf);
    gc_pointer_assign(&shared, leaf);

    // Keep the graph reachable through root and a registered alias to the
    // shared child. Both roots and all pointer fields must be rewritten if a
    // copying collector relocates the graph.
    gc_pointer_assign(&left, NULL);
    gc_pointer_assign(&right, NULL);
    gc_pointer_assign(&leaf, NULL);

    check_graph(root, shared);
    for (size_t round = 0; round < 4; round++) {
        gc_collect();

        check_graph(root, shared);
        TEST_CHECK(root->left->left == shared);
    }

    // The graph remains reachable through root after the extra registered
    // alias is released, so the shared child must still be visited by metadata.
    gc_pointer_assign(&shared, NULL);
    for (size_t round = 0; round < 4; round++) {
        gc_collect();
        check_graph(root, root->left->left);
    }

    gc_pointer_assign(&root, NULL);
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Registered graph test passed!");
    return 0;
}
