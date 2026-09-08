#include "../test_check.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>

struct graph_node {
    int value;
    struct graph_node *left;
    struct graph_node *right;
};

static const size_t graph_node_pointer_offsets[] = {
    offsetof(struct graph_node, left),
    offsetof(struct graph_node, right),
};
static const gc_type_descriptor graph_node_type = {
    sizeof(struct graph_node),
    sizeof(graph_node_pointer_offsets) / sizeof(graph_node_pointer_offsets[0]),
    graph_node_pointer_offsets,
};

static void allocate_node(struct graph_node **slot, int value) {
    gc_pointer_assign(slot, gc_alloc_object(&graph_node_type));
    TEST_CHECK(*slot != NULL);
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

    allocate_node(&root, 1);
    allocate_node(&left, 2);
    allocate_node(&right, 3);
    allocate_node(&leaf, 4);

    gc_pointer_assign(&root->left, left);
    gc_pointer_assign(&root->right, right);
    gc_pointer_assign(&left->left, leaf);
    gc_pointer_assign(&right->right, leaf);
    gc_pointer_assign(&shared, leaf);

    // Keep the graph reachable through root and a rooted alias to the shared
    // child. Both roots and all pointer fields must be rewritten if a copying
    // collector relocates the graph.
    gc_pointer_assign(&left, NULL);
    gc_pointer_assign(&right, NULL);
    gc_pointer_assign(&leaf, NULL);

    check_graph(root, shared);
    for (size_t round = 0; round < 4; round++) {
        gc_collect();

        check_graph(root, shared);
        TEST_CHECK(root->left->left == shared);
    }

    // The graph remains reachable through root after the extra rooted alias is
    // released, so the shared child must still be visited by metadata.
    gc_pointer_assign(&shared, NULL);
    for (size_t round = 0; round < 4; round++) {
        gc_collect();
        check_graph(root, root->left->left);
    }

    gc_pointer_assign(&root, NULL);
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    puts("Typed graph test passed!");
    return 0;
}
