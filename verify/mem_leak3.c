#include "gc.h"

#include <assert.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

gc_ptr_table *ptr_map = NULL;
gc_ptr_table *ptr_map_array = NULL;
void construct_ptr_table_array(void) {
    const size_t positions[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    ptr_map_array = gc_ptr_table_create(10, sizeof(struct tree_node), 2, positions);
    assert(ptr_map_array != NULL);
}
void construct_ptr_table(void) {
    const size_t positions[] = {
        offsetof(struct tree_node, left),
        offsetof(struct tree_node, right),
    };
    ptr_map = gc_ptr_table_create(1, sizeof(struct tree_node), 2, positions);
    assert(ptr_map != NULL);
}

void func(void) {
    construct_ptr_table_array();
    construct_ptr_table();

    gc_scope_token scope = gc_scope_begin();
    struct tree_node *root;
    gc_local_var(&root);

    gc_ptr_copy(&root, gc_malloc(sizeof(struct tree_node) * 10));
    // root points to an array of structures.
    gc_register(root, ptr_map_array);
    assert(root != NULL);

    for (int i = 0; i < 10; i++) {
        root[i].data = i;
        gc_ptr_copy(&root[i].left, gc_malloc(sizeof(struct tree_node)));
        // root[i].left points to a structure.
        gc_register(root[i].left, ptr_map);
        assert(root[i].left != NULL);
        root[i].left->data = i + 1;
        gc_ptr_copy(&root[i].right, gc_malloc(sizeof(struct tree_node)));
        // root[i].right points to a structure.
        gc_register(root[i].right, ptr_map);
        assert(root[i].right != NULL);
        root[i].right->data = i + 2;
    }

    gc_scope_end(scope);
}

int main(void) {
    gc_init();

    int prev_free_size = gc_free_size();

    func();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the leaked size.
    int curr_free_size = gc_free_size();
    int leak_size = prev_free_size - curr_free_size;

    // Assert that no memory was leaked.
    assert(leak_size == 0);

    gc_cleanup();
    gc_ptr_table_destroy(ptr_map);
    gc_ptr_table_destroy(ptr_map_array);

    return 0;
}
