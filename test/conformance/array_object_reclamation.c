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

void allocate_unrooted_tree_array(void) {
    gc_scope_token scope = gc_scope_begin();
    struct tree_node *root;
    struct tree_node *temporary;
    gc_scope_add_root(&root);
    gc_scope_add_root(&temporary);

    gc_pointer_assign(&root, gc_alloc_array(&tree_node_type, 10));
    // root points to an array of structures.
    TEST_CHECK(root != NULL);

    for (int i = 0; i < 10; i++) {
        root[i].data = i;
        gc_pointer_assign(&temporary, gc_alloc_object(&tree_node_type));
        // Each left field points to a separately allocated structure.
        TEST_CHECK(temporary != NULL);
        temporary->data = i + 1;
        // Force a moving safe point before forming the destination field address.
        gc_collect();
        gc_pointer_assign(&root[i].left, temporary);
        gc_pointer_assign(&temporary, NULL);

        gc_pointer_assign(&temporary, gc_alloc_object(&tree_node_type));
        // Each right field points to a separately allocated structure.
        TEST_CHECK(temporary != NULL);
        temporary->data = i + 2;
        // Force a moving safe point before forming the destination field address.
        gc_collect();
        gc_pointer_assign(&root[i].right, temporary);
        gc_pointer_assign(&temporary, NULL);
    }

    gc_scope_end(scope);
}

int main(void) {
    gc_init();

    size_t free_bytes_before = test_gc_free_bytes();

    allocate_unrooted_tree_array();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the bytes that remained unreclaimed.
    size_t free_bytes_after = test_gc_free_bytes();
    size_t unreclaimed_bytes = free_bytes_before - free_bytes_after;

    // Assert that the unrooted array and child objects were reclaimed.
    TEST_CHECK(unreclaimed_bytes == 0);

    gc_cleanup();
    return 0;
}
