#include "gc.h"

#include <stdlib.h>

int _main() {
    int *ptr;

    // Allocate memory normally.
    ptr = malloc(sizeof(int) * 10);

    // Free the memory.
    free(ptr);

    // Free the memory again.
    free(ptr);

    return 0;
}

int main() {
    gc_init();

    int *ptr;
    gc_local_var(&ptr);

    // Allocate memory normally.
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    // No manual free is needed because the GC handles reclamation.

    gc_pop();
    gc_cleanup();

    return 0;
}
