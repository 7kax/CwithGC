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

static gc_ptr_table *create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct node, left),
        offsetof(struct node, right),
    };
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct node), 2, pointer_field_offsets);
    assert(table != NULL);
    return table;
}

static struct node *make_node(int value, gc_ptr_table *pointer_table) {
    struct node *node = gc_malloc(sizeof(struct node));
    gc_register_object(node, pointer_table);
    node->value = value;
    return node;
}

int main(void) {
    gc_ptr_table *pointer_table = create_pointer_table();
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *root, *left, *right, *leaf;
    gc_scope_add_root((void **)&root);
    gc_scope_add_root((void **)&left);
    gc_scope_add_root((void **)&right);
    gc_scope_add_root((void **)&leaf);

    gc_pointer_assign((void **)&root, make_node(1, pointer_table));
    gc_pointer_assign((void **)&left, make_node(2, pointer_table));
    gc_pointer_assign((void **)&right, make_node(3, pointer_table));
    gc_pointer_assign((void **)&leaf, make_node(4, pointer_table));

    gc_pointer_assign((void **)&root->left, left);
    gc_pointer_assign((void **)&root->right, right);
    gc_pointer_assign((void **)&left->left, leaf);
    gc_pointer_assign((void **)&right->right, leaf);
    gc_pointer_assign((void **)&leaf->left, root);

    // Keep the graph alive solely through root. The leaf is shared by two
    // parents and points back to root, exercising both aliasing and a cycle.
    gc_pointer_assign((void **)&left, NULL);
    gc_pointer_assign((void **)&right, NULL);
    gc_pointer_assign((void **)&leaf, NULL);

    const size_t live_size = test_gc_heap_capacity() - test_gc_free_bytes();
    for (size_t round = 0; round < 4; round++) {
        struct node *old_root = root;
        struct node *old_left = root->left;
        struct node *old_right = root->right;
        struct node *old_leaf = root->left->left;

        gc_collect();

        assert(test_gc_relocated_block_count() == 4 * (round + 1));
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

    gc_pointer_assign((void **)&root, NULL);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(pointer_table);

    puts("Copying graph test passed!");
    return 0;
}
