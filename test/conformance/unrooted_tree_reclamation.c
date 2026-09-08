#include "../test_debug.h"
#include "gc.h"

#include <stddef.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

static const size_t tree_node_pointer_offsets[] = {
    offsetof(struct tree_node, left),
    offsetof(struct tree_node, right),
};
static const gc_type_descriptor tree_node_type = {
    sizeof(struct tree_node),
    sizeof(tree_node_pointer_offsets) / sizeof(tree_node_pointer_offsets[0]),
    tree_node_pointer_offsets,
};

void allocate_unrooted_tree(void) {
    gc_scope_token scope = gc_scope_begin();
    struct tree_node *root;
    struct tree_node *temporary;
    gc_scope_add_root(&root);
    gc_scope_add_root(&temporary);

    gc_pointer_assign(&root, gc_alloc_object(&tree_node_type));
    TEST_CHECK(root != NULL);

    root->data = 1;
    gc_pointer_assign(&temporary, gc_alloc_object(&tree_node_type));
    TEST_CHECK(temporary != NULL);
    temporary->data = 2;
    // Force a moving safe point before forming the destination field address.
    gc_collect();
    gc_pointer_assign(&root->left, temporary);
    gc_pointer_assign(&temporary, NULL);

    gc_pointer_assign(&temporary, gc_alloc_object(&tree_node_type));
    TEST_CHECK(temporary != NULL);
    temporary->data = 3;
    // Force a moving safe point before forming the destination field address.
    gc_collect();
    gc_pointer_assign(&root->right, temporary);
    gc_pointer_assign(&temporary, NULL);

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
    TEST_CHECK(unreclaimed_bytes == 0);

    gc_cleanup();
    return 0;
}
