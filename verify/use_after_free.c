#include "gc.h"

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    int *ptr;
    gc_scope_add_root(&ptr);

    // Allocate memory normally.
    gc_pointer_assign(&ptr, gc_malloc(sizeof(int) * 10));

    int *ptr2;
    gc_scope_add_root(&ptr2);

    // Point another pointer at the same memory.
    gc_pointer_assign(&ptr2, ptr);

    // No manual free is needed because the GC handles reclamation.

    // ptr2 is safe to use here.
    *ptr2 = 42; // Use ptr2.

    gc_scope_end(scope);
    gc_cleanup();

    return 0;
}
