#include "../test_debug.h"
#include "gc.h"

#include <stdio.h>
#include <stdlib.h>

struct node {
    int value;
    struct node *left;
    struct node *right;
};

static const size_t node_pointer_offsets[] = {
    offsetof(struct node, left),
    offsetof(struct node, right),
};
static const gc_type_descriptor node_type = {
    sizeof(struct node),
    sizeof(node_pointer_offsets) / sizeof(node_pointer_offsets[0]),
    node_pointer_offsets,
};

static struct node *make_node(int value) {
    struct node *node = gc_alloc_object(&node_type);
    node->value = value;
    return node;
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *root, *left, *right, *leaf;
    gc_scope_add_root(&root);
    gc_scope_add_root(&left);
    gc_scope_add_root(&right);
    gc_scope_add_root(&leaf);

    gc_pointer_assign(&root, make_node(1));
    gc_pointer_assign(&left, make_node(2));
    gc_pointer_assign(&right, make_node(3));
    gc_pointer_assign(&leaf, make_node(4));

    gc_pointer_assign(&root->left, left);
    gc_pointer_assign(&root->right, right);
    gc_pointer_assign(&left->left, leaf);
    gc_pointer_assign(&right->right, leaf);
    gc_pointer_assign(&leaf->left, root);

    // Keep the graph alive solely through root. The leaf is shared by two
    // parents and points back to root, exercising both aliasing and a cycle.
    gc_pointer_assign(&left, NULL);
    gc_pointer_assign(&right, NULL);
    gc_pointer_assign(&leaf, NULL);

    const size_t live_size = test_gc_heap_capacity() - test_gc_free_bytes();
    for (size_t round = 0; round < 4; round++) {
        gc_collect();

        TEST_CHECK(test_gc_relocated_block_count() == 4 * (round + 1));
        TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity() - live_size);

        TEST_CHECK(root->value == 1);
        TEST_CHECK(root->left->value == 2);
        TEST_CHECK(root->right->value == 3);
        TEST_CHECK(root->left->left->value == 4);
        TEST_CHECK(root->left->left == root->right->right);
        TEST_CHECK(root->left->left->left == root);
    }

    gc_pointer_assign(&root, NULL);
    gc_collect();
    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);
    gc_cleanup();
    puts("Copying graph test passed!");
    return 0;
}
