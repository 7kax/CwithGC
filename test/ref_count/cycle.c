#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

struct node {
    struct node *next;
};

int main(void) {
    const size_t positions[] = {offsetof(struct node, next)};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct node), 1, positions);
    assert(table != NULL);

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *first;
    struct node *second;
    gc_local_var(&first);
    gc_local_var(&second);

    gc_ptr_copy(&first, gc_malloc(sizeof(*first)));
    gc_register(first, table);
    gc_ptr_copy(&second, gc_malloc(sizeof(*second)));
    gc_register(second, table);

    gc_ptr_copy(&first->next, second);
    gc_ptr_copy(&second->next, first);

    // Drop the external references while the two nodes retain each other.
    gc_ptr_copy(&first, NULL);
    gc_ptr_copy(&second, NULL);
    gc_scope_end(scope);
    assert(test_gc_root_count() == 0);

    const size_t free_size = test_gc_free_bytes();
    const size_t collected = test_gc_reclaimed_blocks();
    assert(free_size < test_gc_heap_capacity());

    gc_collect();

    // Reference counting does not trace unreachable cycles during collection.
    assert(test_gc_free_bytes() == free_size);
    assert(test_gc_reclaimed_blocks() == collected);

    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Reference counting cycle test passed!");
    return 0;
}
