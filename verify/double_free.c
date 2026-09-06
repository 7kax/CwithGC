#include "gc.h"

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    int *ptr;
    gc_local_var(&ptr);

    // Allocate memory normally.
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    // No manual free is needed because the GC handles reclamation.

    gc_scope_end(scope);
    gc_cleanup();

    return 0;
}
