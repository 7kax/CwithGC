#include "../test/test_debug.h"
#include "gc.h"

#include <assert.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

gc_ptr_table *pointer_table = NULL;
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
void create_pointer_table(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    pointer_table = gc_ptr_table_create(1, sizeof(struct tree_node), 2, pointer_field_offsets);
    assert(pointer_table != NULL);
}

void allocate_unrooted_tree(void) {
    create_array_pointer_table();
    create_pointer_table();

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
        // root[i].left points to a structure.
        gc_register_object(root[i].left, pointer_table);
        assert(root[i].left != NULL);
        root[i].left->data = i + 1;
        gc_pointer_assign(&root[i].right, gc_malloc(sizeof(struct tree_node)));
        // root[i].right points to a structure.
        gc_register_object(root[i].right, pointer_table);
        assert(root[i].right != NULL);
        root[i].right->data = i + 2;
    }

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
    gc_ptr_table_destroy(array_pointer_table);

    return 0;
}
