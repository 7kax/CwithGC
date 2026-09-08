#include "../test_debug.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>

struct node {
    struct node *next;
};

static const size_t node_pointer_offsets[] = {offsetof(struct node, next)};
static const gc_type_descriptor node_type = {
    sizeof(struct node),
    sizeof(node_pointer_offsets) / sizeof(node_pointer_offsets[0]),
    node_pointer_offsets,
};

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *first;
    struct node *second;
    gc_scope_add_root(&first);
    gc_scope_add_root(&second);

    gc_pointer_assign(&first, gc_alloc_object(&node_type));
    gc_pointer_assign(&second, gc_alloc_object(&node_type));

    gc_pointer_assign(&first->next, second);
    gc_pointer_assign(&second->next, first);

    // Drop the external references while the two nodes retain each other.
    gc_pointer_assign(&first, NULL);
    gc_pointer_assign(&second, NULL);
    gc_scope_end(scope);
    TEST_CHECK(test_gc_root_count() == 0);

    const size_t free_bytes = test_gc_free_bytes();
    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    TEST_CHECK(free_bytes < test_gc_heap_capacity());

    gc_collect();

    // Reference counting does not trace unreachable cycles during collection.
    TEST_CHECK(test_gc_free_bytes() == free_bytes);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count);

    gc_cleanup();
    puts("Reference counting cycle test passed!");
    return 0;
}
