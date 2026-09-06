#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct node {
    int value;
    struct node *left;
    struct node *right;
};

static gc_ptr_table *construct_ptr_table(void) {
    const size_t positions[] = {
        offsetof(struct node, left),
        offsetof(struct node, right),
    };
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct node), 2, positions);
    assert(table != NULL);
    return table;
}

static struct node *make_node(int value, gc_ptr_table *ptr_table) {
    struct node *node = gc_malloc(sizeof(struct node));
    gc_register(node, ptr_table);
    node->value = value;
    return node;
}

int main(void) {
    gc_ptr_table *ptr_table = construct_ptr_table();
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *root, *left, *right, *leaf;
    gc_local_var((void **)&root);
    gc_local_var((void **)&left);
    gc_local_var((void **)&right);
    gc_local_var((void **)&leaf);

    gc_ptr_copy((void **)&root, make_node(1, ptr_table));
    gc_ptr_copy((void **)&left, make_node(2, ptr_table));
    gc_ptr_copy((void **)&right, make_node(3, ptr_table));
    gc_ptr_copy((void **)&leaf, make_node(4, ptr_table));

    gc_ptr_copy((void **)&root->left, left);
    gc_ptr_copy((void **)&root->right, right);
    gc_ptr_copy((void **)&left->left, leaf);
    gc_ptr_copy((void **)&right->right, leaf);
    gc_ptr_copy((void **)&leaf->left, root);

    // Keep the graph alive solely through root. The leaf is shared by two
    // parents and points back to root, exercising both aliasing and a cycle.
    gc_ptr_copy((void **)&left, NULL);
    gc_ptr_copy((void **)&right, NULL);
    gc_ptr_copy((void **)&leaf, NULL);

    const size_t live_size = test_gc_heap_capacity() - test_gc_free_bytes();
    for (size_t round = 0; round < 4; round++) {
        struct node *old_root = root;
        struct node *old_left = root->left;
        struct node *old_right = root->right;
        struct node *old_leaf = root->left->left;

        gc_collect();

        assert(test_gc_reclaimed_blocks() == 4 * (round + 1));
        assert(test_gc_free_bytes() == test_gc_heap_capacity() - live_size);
        assert(root != old_root);
        assert(root->left != old_left);
        assert(root->right != old_right);
        assert(root->left->left != old_leaf);

        assert(root->value == 1);
        assert(root->left->value == 2);
        assert(root->right->value == 3);
        assert(root->left->left->value == 4);
        assert(root->left->left == root->right->right);
        assert(root->left->left->left == root);
    }

    gc_ptr_copy((void **)&root, NULL);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(ptr_table);

    puts("Copying graph test passed!");
    return 0;
}
