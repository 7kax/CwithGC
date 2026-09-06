#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct node {
    int data;
    struct node *next;
};

gc_ptr_table *construct_ptr_table(void) {
    const size_t positions[] = {offsetof(struct node, next)};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct node), 1, positions);
    assert(table != NULL);
    return table;
}

gc_ptr_table *ptr_map = NULL;

struct node *make_node(int data) {
    gc_scope_token scope = gc_scope_begin();
    struct node *new_node;
    gc_local_var(&new_node);

    gc_ptr_copy(&new_node, gc_malloc(sizeof(struct node)));
    gc_register(new_node, ptr_map);

    new_node->data = data;

    gc_scope_end(scope);

    return new_node;
}

int main(void) {
    int elements[] = {1, 2, 3, 4, 5};
    int n = 5;
    ptr_map = construct_ptr_table();

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *head, *cur, *new_node;
    gc_local_var(&head);
    gc_local_var(&cur);
    gc_local_var(&new_node);

    gc_ptr_copy(&head, make_node(elements[0]));
    gc_ptr_copy(&cur, head);

    // Build a linked list.
    for (int i = 1; i < n; i++) {
        gc_ptr_copy(&new_node, make_node(elements[i]));
        gc_ptr_copy(&(cur->next), new_node);
        gc_ptr_copy(&cur, new_node);
    }

    // Only the head keeps the list alive during collection.
    gc_ptr_copy(&cur, NULL);
    gc_ptr_copy(&new_node, NULL);

    const size_t live_size = gc_heap_size() - gc_free_size();
    for (int round = 0; round < 3; round++) {
        struct node *old_nodes[5];
        struct node *node = head;
        for (int i = 0; i < n; i++) {
            assert(node != NULL);
            old_nodes[i] = node;
            node = node->next;
        }
        assert(node == NULL);

        gc_collect();
        assert(gc_block_collected() == (size_t)n * (round + 1));
        assert(gc_free_size() == gc_heap_size() - live_size);

        node = head;
        for (int i = 0; i < n; i++) {
            assert(node != NULL);
            assert(node != old_nodes[i]);
            assert(node->data == elements[i]);
            node = node->next;
        }
        assert(node == NULL);
    }

    gc_ptr_copy((void **)&head, NULL);
    assert(cur == NULL);
    assert(new_node == NULL);

    // Every node should now be reclaimed.
    gc_collect();
    assert(gc_free_size() == gc_heap_size());

    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(ptr_map);
    puts("Copying linked list test passed!");

    return 0;
}
