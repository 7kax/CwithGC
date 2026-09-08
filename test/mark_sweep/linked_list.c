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
    const size_t n = 5;
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *head, *cur, *new_node;
    gc_scope_add_root(&head);
    gc_scope_add_root(&cur);
    gc_scope_add_root(&new_node);

    gc_pointer_assign(&head, make_node(elements[0]));
    gc_pointer_assign(&cur, head);

    for (size_t i = 1; i < n; i++) {
        gc_pointer_assign(&new_node, make_node(elements[i]));
        gc_pointer_assign(&(cur->next), new_node);
        gc_pointer_assign(&cur, new_node);
    }

    gc_pointer_assign(&cur, NULL);
    gc_pointer_assign(&new_node, NULL);

    for (size_t i = 0; i < n; i++) {
        TEST_CHECK(head->data == elements[i]);
        gc_pointer_assign(&head, head->next);
    }

    TEST_CHECK(head == NULL);
    TEST_CHECK(cur == NULL);
    TEST_CHECK(new_node == NULL);

    gc_collect();
    TEST_CHECK(test_gc_reclaimed_block_count() == n);

    gc_scope_end(scope);
    gc_cleanup();
    puts("Mark-sweep linked list test passed!");

    return 0;
}
