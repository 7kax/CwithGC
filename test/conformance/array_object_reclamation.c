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
gc_ptr_table *array_pointer_table = NULL;
void create_array_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    array_pointer_table =
        gc_ptr_table_create(10, sizeof(struct tree_node), 2, pointer_field_offsets);
    assert(array_pointer_table != NULL);
}
void create_tree_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    tree_pointer_table = gc_ptr_table_create(1, sizeof(struct tree_node), 2, pointer_field_offsets);
    assert(tree_pointer_table != NULL);
}

void allocate_unrooted_tree_array(void) {
    create_array_pointer_table();
    create_tree_pointer_table();

    gc_scope_token scope = gc_scope_begin();
    struct tree_node *root;
    gc_scope_add_root(&root);

    gc_pointer_assign(&root, gc_malloc(sizeof(struct tree_node) * 10));
    // root points to an array of structures.
    gc_register_object(root, array_pointer_table);
    assert(root != NULL);

    for (int i = 0; i < 10; i++) {
        root[i].data = i;
        gc_pointer_assign(&root[i].left, gc_malloc(sizeof(struct tree_node)));
        // Each left field points to a separately allocated structure.
        gc_register_object(root[i].left, tree_pointer_table);
        assert(root[i].left != NULL);
        root[i].left->data = i + 1;
        gc_pointer_assign(&root[i].right, gc_malloc(sizeof(struct tree_node)));
        // Each right field points to a separately allocated structure.
        gc_register_object(root[i].right, tree_pointer_table);
        assert(root[i].right != NULL);
        root[i].right->data = i + 2;
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
    assert(unreclaimed_bytes == 0);

    gc_cleanup();
    gc_ptr_table_destroy(tree_pointer_table);
    gc_ptr_table_destroy(array_pointer_table);

    return 0;
}
