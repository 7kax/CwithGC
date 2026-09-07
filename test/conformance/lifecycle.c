#include "../test_check.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>

int main(void) {
    // Cleanup is idempotent, including before the first initialization.
    gc_cleanup();
    gc_cleanup();

    gc_init();

    gc_scope_token first_scope = gc_scope_begin();
    void *first_root;
    gc_scope_add_root(&first_root);
    gc_pointer_assign(&first_root, gc_malloc(32));
    TEST_CHECK(first_root != NULL);
    gc_collect();
    TEST_CHECK(first_root != NULL);
    gc_scope_end(first_scope);

    // Reinitialization releases the old heap/allocations and root storage.
    gc_init();

    void *second_root;
    gc_scope_token second_scope = gc_scope_begin();
    gc_scope_add_root(&second_root);
    gc_pointer_assign(&second_root, gc_malloc(64));
    TEST_CHECK(second_root != NULL);
    gc_collect();
    TEST_CHECK(second_root != NULL);
    gc_scope_end(second_scope);

    // Cleanup also releases objects that are still reachable.
    gc_cleanup();
    gc_cleanup();
    gc_init();
    gc_collect();
    gc_cleanup();

    puts("GC lifecycle test passed!");
    return 0;
}
