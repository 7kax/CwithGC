#include "gc.h"

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    int *ptr;
    gc_scope_add_root(&ptr);

    // Allocate memory normally.
    gc_pointer_assign(&ptr, gc_malloc(sizeof(int) * 10));

    // No manual free is needed because the GC handles reclamation.

    // ptr remains valid here.
    *ptr = 42; // Use ptr.

    gc_scope_end(scope);
    gc_cleanup();

    return 0;
}
