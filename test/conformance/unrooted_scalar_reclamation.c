#include "../test_debug.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>

void allocate_unrooted_scalar(void) {
    gc_scope_token scope = gc_scope_begin();
    int *ptr;
    gc_scope_add_root(&ptr);

    gc_pointer_assign(&ptr, gc_malloc(sizeof(int) * 10));

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    size_t free_bytes_before = test_gc_free_bytes();

    allocate_unrooted_scalar();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the bytes that remained unreclaimed.
    size_t free_bytes_after = test_gc_free_bytes();
    size_t unreclaimed_bytes = free_bytes_before - free_bytes_after;

    // Assert that the unrooted allocation was reclaimed.
    TEST_CHECK(unreclaimed_bytes == 0);

    gc_scope_end(scope);

    gc_cleanup();

    return 0;
}
