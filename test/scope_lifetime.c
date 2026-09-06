#include "gc.h"
#include "test_debug.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static void nested_scopes(void) {
    const gc_scope_token outer_scope = gc_scope_begin();
    int *outer_root;
    gc_local_var(&outer_root);
    gc_ptr_copy(&outer_root, gc_malloc(sizeof(int)));
    *outer_root = 10;

    const gc_scope_token inner_scope = gc_scope_begin();
    int *inner_root;
    gc_local_var(&inner_root);
    gc_ptr_copy(&inner_root, gc_malloc(sizeof(int)));
    *inner_root = 20;

    assert(inner_scope != outer_scope);
    assert(test_gc_root_count() == 2);
    gc_scope_end(inner_scope);
    assert(test_gc_root_count() == 1);

    gc_collect();
    assert(*outer_root == 10);

    gc_scope_end(outer_scope);
    assert(test_gc_root_count() == 0);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());
}

static void recursive_scopes(size_t remaining, size_t active_scopes) {
    const gc_scope_token scope = gc_scope_begin();
    int *root;
    gc_local_var(&root);
    gc_ptr_copy(&root, gc_malloc(sizeof(int)));
    *root = (int)remaining;

    assert(test_gc_root_count() == active_scopes);
    if (remaining != 0)
        recursive_scopes(remaining - 1, active_scopes + 1);

    assert(test_gc_root_count() == active_scopes);
    gc_collect();
    assert(*root == (int)remaining);
    gc_scope_end(scope);
    assert(test_gc_root_count() == active_scopes - 1);
}

int main(void) {
    gc_init();

    nested_scopes();
    recursive_scopes(7, 1);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_cleanup();
    puts("GC scope lifetime test passed!");
    return 0;
}
