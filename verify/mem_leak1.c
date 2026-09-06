#include "../test/test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>

void func(void) {
    gc_scope_token scope = gc_scope_begin();
    int *ptr;
    gc_local_var(&ptr);

    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    gc_scope_end(scope);
}

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    int prev_free_size = test_gc_free_bytes();

    func();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the leaked size.
    int curr_free_size = test_gc_free_bytes();
    int leak_size = prev_free_size - curr_free_size;

    // Assert that no memory was leaked.
    assert(leak_size == 0);

    gc_scope_end(scope);

    gc_cleanup();

    return 0;
}
