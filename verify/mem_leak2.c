#include "../test/test_debug.h"
#include "gc.h"

#include <assert.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

gc_ptr_table *pointer_table = NULL;
void create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    pointer_table = gc_ptr_table_create(1, sizeof(struct tree_node), 2, pointer_field_offsets);
    assert(pointer_table != NULL);
}
void allocate_unrooted_tree(void) {
    create_pointer_table();

    gc_scope_token scope = gc_scope_begin();
    struct tree_node *root;
    gc_scope_add_root(&root);

    gc_pointer_assign(&root, gc_malloc(sizeof(struct tree_node)));
    gc_register_object(root, pointer_table);
    assert(root != NULL);

    root->data = 1;
    gc_pointer_assign(&root->left, gc_malloc(sizeof(struct tree_node)));
    gc_register_object(root->left, pointer_table);
    assert(root->left != NULL);
    root->left->data = 2;
    gc_pointer_assign(&root->right, gc_malloc(sizeof(struct tree_node)));
    gc_register_object(root->right, pointer_table);
    assert(root->right != NULL);
    root->right->data = 3;

    // root, root->left, and root->right are not freed here.

    gc_scope_end(scope);
}

int main(void) {
    gc_init();

    int previous_free_bytes = test_gc_free_bytes();

    allocate_unrooted_tree();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the leaked size.
    int current_free_bytes = test_gc_free_bytes();
    int leaked_bytes = previous_free_bytes - current_free_bytes;

    // Assert that no memory was leaked.
    assert(leaked_bytes == 0);

    gc_cleanup();
    gc_ptr_table_destroy(pointer_table);

    return 0;
}
