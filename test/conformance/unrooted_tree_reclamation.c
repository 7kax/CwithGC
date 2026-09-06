#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stddef.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

gc_ptr_table *tree_pointer_table = NULL;
void create_tree_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    tree_pointer_table = gc_ptr_table_create(1, sizeof(struct tree_node), 2, pointer_field_offsets);
    assert(tree_pointer_table != NULL);
}
void allocate_unrooted_tree(void) {
    create_tree_pointer_table();

    gc_scope_token scope = gc_scope_begin();
    struct tree_node *root;
    gc_scope_add_root(&root);

    gc_pointer_assign(&root, gc_malloc(sizeof(struct tree_node)));
    gc_register_object(root, tree_pointer_table);
    assert(root != NULL);

    root->data = 1;
    gc_pointer_assign(&root->left, gc_malloc(sizeof(struct tree_node)));
    gc_register_object(root->left, tree_pointer_table);
    assert(root->left != NULL);
    root->left->data = 2;
    gc_pointer_assign(&root->right, gc_malloc(sizeof(struct tree_node)));
    gc_register_object(root->right, tree_pointer_table);
    assert(root->right != NULL);
    root->right->data = 3;

    // The scope ends without retaining a root for any of the three allocations.

    gc_scope_end(scope);
}

int main(void) {
    gc_init();

    size_t free_bytes_before = test_gc_free_bytes();

    allocate_unrooted_tree();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the bytes that remained unreclaimed.
    size_t free_bytes_after = test_gc_free_bytes();
    size_t unreclaimed_bytes = free_bytes_before - free_bytes_after;

    // Assert that the unrooted tree was reclaimed.
    assert(unreclaimed_bytes == 0);

    gc_cleanup();
    gc_ptr_table_destroy(tree_pointer_table);

    return 0;
}
