#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdlib.h>

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    void *root;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_malloc(test_gc_heap_capacity() - test_gc_metadata_size()));
    assert(test_gc_free_bytes() == 0);

    // Collection cannot reclaim the live full-heap object, so this request
    // must reach the runtime allocation-failure path instead of an assertion.
    (void)gc_malloc(1);

    gc_scope_end(scope);
    gc_cleanup();

    return EXIT_FAILURE;
}
