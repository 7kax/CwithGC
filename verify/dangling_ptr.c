#include "gc.h"

int main(void) {
    gc_init();

    int *ptr;
    gc_local_var(&ptr);

    // Allocate memory normally.
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    // No manual free is needed because the GC handles reclamation.

    // ptr remains valid here.
    *ptr = 42; // Use ptr.

    gc_pop();
    gc_cleanup();

    return 0;
}
