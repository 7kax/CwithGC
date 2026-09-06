#include "gc.h"

#include <assert.h>
#include <signal.h>
#include <stdlib.h>

static void handle_abort(int signal_number) {
    if (signal_number == SIGABRT)
        _Exit(EXIT_SUCCESS);
}

int main(void) {
    signal(SIGABRT, handle_abort);
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    void *root;
    gc_local_var(&root);
    gc_ptr_copy(&root, gc_malloc(gc_heap_size() - gc_meta_size()));
    assert(gc_free_size() == 0);

    // Collection cannot reclaim the live full-heap object, so this request
    // must reach gc_allocation_failure() instead of an internal assertion.
    (void)gc_malloc(1);

    gc_scope_end(scope);
    gc_cleanup();

    return EXIT_FAILURE;
}
