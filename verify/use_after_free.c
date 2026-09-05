#include "gc.h"

#include <stdlib.h>

int _main() {
    int *ptr;

    // Allocate memory normally.
    ptr = malloc(sizeof(int) * 10);

    // Point another pointer at the same memory.
    int *ptr2 = ptr;

    // Free ptr.
    free(ptr);

    // Access the memory through ptr2.
    // Accessing freed memory here should cause undefined behavior.
    *ptr2 = 42;

    return 0;
}

int main() {
    gc_init();

    int *ptr;
    gc_local_var(&ptr);

    // Allocate memory normally.
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    int *ptr2;
    gc_local_var(&ptr2);

    // Point another pointer at the same memory.
    gc_ptr_copy(&ptr2, ptr);

    // No manual free is needed because the GC handles reclamation.

    // ptr2 is safe to use here.
    *ptr2 = 42; // Use ptr2.

    gc_pop();
    gc_cleanup();

    return 0;
}
