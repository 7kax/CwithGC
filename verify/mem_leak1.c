#include "../test/test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

void allocate_unrooted_object(void) {
    gc_scope_token scope = gc_scope_begin();
    int *ptr;
    gc_scope_add_root(&ptr);

    gc_pointer_assign(&ptr, gc_malloc(sizeof(int) * 10));

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    int previous_free_bytes = test_gc_free_bytes();

    allocate_unrooted_object();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the leaked size.
    int current_free_bytes = test_gc_free_bytes();
    int leaked_bytes = previous_free_bytes - current_free_bytes;

    // Assert that no memory was leaked.
    assert(leaked_bytes == 0);

    gc_scope_end(scope);

    gc_cleanup();

    return 0;
}
