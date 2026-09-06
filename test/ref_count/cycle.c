#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

struct node {
    struct node *next;
};

int main(void) {
    const size_t pointer_field_offsets[] = {offsetof(struct node, next)};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct node), 1, pointer_field_offsets);
    assert(table != NULL);

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct node *first;
    struct node *second;
    gc_scope_add_root(&first);
    gc_scope_add_root(&second);

    gc_pointer_assign(&first, gc_malloc(sizeof(*first)));
    gc_register_object(first, table);
    gc_pointer_assign(&second, gc_malloc(sizeof(*second)));
    gc_register_object(second, table);

    gc_pointer_assign(&first->next, second);
    gc_pointer_assign(&second->next, first);

    // Drop the external references while the two nodes retain each other.
    gc_pointer_assign(&first, NULL);
    gc_pointer_assign(&second, NULL);
    gc_scope_end(scope);
    assert(test_gc_root_count() == 0);

    const size_t free_bytes = test_gc_free_bytes();
    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    assert(free_bytes < test_gc_heap_capacity());

    gc_collect();

    // Reference counting does not trace unreachable cycles during collection.
    assert(test_gc_free_bytes() == free_bytes);
    assert(test_gc_reclaimed_block_count() == reclaimed_count);

    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Reference counting cycle test passed!");
    return 0;
}
