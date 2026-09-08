#include "../test_debug.h"
#include "gc.h"

#include <stdio.h>
#include <stdlib.h>

struct node {
    int data;
    struct node *next;
};

static const size_t node_pointer_offsets[] = {offsetof(struct node, next)};
static const gc_type_descriptor node_type = {
    sizeof(struct node),
    sizeof(node_pointer_offsets) / sizeof(node_pointer_offsets[0]),
    node_pointer_offsets,
};

struct node *make_node(int data) {
    gc_scope_token scope = gc_scope_begin();
    struct node *new_node;
    gc_scope_add_root(&new_node);

    gc_pointer_assign(&new_node, gc_alloc_object(&node_type));

    new_node->data = data;

    gc_scope_end(scope);

    return new_node;
}

int main(void) {
    int elements[] = {1, 2, 3, 4, 5};
    int n = 5;
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *head, *cur, *new_node;
    gc_scope_add_root(&head);
    gc_scope_add_root(&cur);
    gc_scope_add_root(&new_node);

    gc_pointer_assign(&head, make_node(elements[0]));
    gc_pointer_assign(&cur, head);

    // Build a linked list.
    for (int i = 1; i < n; i++) {
        gc_pointer_assign(&new_node, make_node(elements[i]));
        gc_pointer_assign(&(cur->next), new_node);
        gc_pointer_assign(&cur, new_node);
    }

    // Only the head keeps the list alive during collection.
    gc_pointer_assign(&cur, NULL);
    gc_pointer_assign(&new_node, NULL);

    const size_t live_size = test_gc_heap_capacity() - test_gc_free_bytes();
    for (int round = 0; round < 3; round++) {
        struct node *node = head;
        for (int i = 0; i < n; i++) {
            TEST_CHECK(node != NULL);
            node = node->next;
        }
        TEST_CHECK(node == NULL);

        gc_collect();
        TEST_CHECK(test_gc_relocated_block_count() == (size_t)n * (round + 1));
        TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity() - live_size);

        node = head;
        for (int i = 0; i < n; i++) {
            TEST_CHECK(node != NULL);
            TEST_CHECK(node->data == elements[i]);
            node = node->next;
        }
        TEST_CHECK(node == NULL);
    }

    gc_pointer_assign(&head, NULL);
    TEST_CHECK(cur == NULL);
    TEST_CHECK(new_node == NULL);

    // Every node should now be reclaimed.
    gc_collect();
    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);
    gc_cleanup();
    puts("Copying linked list test passed!");

    return 0;
}
