#include "../test_check.h"
#include "../test_types.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>

static void nested_scopes(void) {
    const gc_scope_token outer_scope = gc_scope_begin();
    int *outer_root;
    gc_scope_add_root(&outer_root);
    gc_pointer_assign(&outer_root, gc_alloc_object(test_gc_int_type()));
    *outer_root = 10;

    const gc_scope_token inner_scope = gc_scope_begin();
    int *inner_root;
    gc_scope_add_root(&inner_root);
    gc_pointer_assign(&inner_root, gc_alloc_object(test_gc_int_type()));
    *inner_root = 20;

    TEST_CHECK(inner_scope != outer_scope);
    gc_scope_end(inner_scope);

    gc_collect();
    TEST_CHECK(*outer_root == 10);

    gc_scope_end(outer_scope);
    gc_collect();
}

static void recursive_scopes(size_t remaining) {
    const gc_scope_token scope = gc_scope_begin();
    int *root;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_alloc_object(test_gc_int_type()));
    *root = (int)remaining;

    if (remaining != 0)
        recursive_scopes(remaining - 1);

    gc_collect();
    TEST_CHECK(*root == (int)remaining);
    gc_scope_end(scope);
}

int main(void) {
    gc_init();

    nested_scopes();
    recursive_scopes(7);
    gc_collect();

    gc_cleanup();
    puts("GC scope lifetime test passed!");
    return 0;
}
