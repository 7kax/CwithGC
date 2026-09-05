#include "gc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int main(void) {
    // Cleanup is idempotent, including before the first initialization.
    gc_cleanup();
    gc_cleanup();

    gc_init();

    void *first_root;
    gc_local_var(&first_root);
    gc_ptr_copy(&first_root, gc_malloc(32));
    assert(gc_free_size() < gc_heap_size());
    assert(gc_root_size() == 1);

    // Reinitialization releases the old heap/allocations and root storage.
    gc_init();
    assert(gc_free_size() == gc_heap_size());
    assert(gc_root_size() == 0);
    assert(gc_block_collected() == 0);

    // Popping an empty root set is a safe no-op.
    gc_pop();
    assert(gc_root_size() == 0);

    void *second_root;
    gc_local_var(&second_root);
    gc_ptr_copy(&second_root, gc_malloc(64));
    assert(gc_root_size() == 1);

    // Cleanup also releases objects that are still reachable.
    gc_cleanup();
    assert(gc_free_size() == 0);
    assert(gc_root_size() == 0);

    gc_cleanup();
    gc_init();
    gc_collect();
    gc_cleanup();

    puts("GC lifecycle test passed!");
    return 0;
}
