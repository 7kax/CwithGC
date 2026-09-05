#include "gc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct foo {
    int *a;
    int *b;
};

// Unsafe version.
int _main() {
    // Local pointer variables that simulate uninitialized state.
    int *ptr1 = (int *)(uintptr_t)0xff;
    int *ptr2 = (int *)(uintptr_t)0xff;
    int *ptr3 = (int *)(uintptr_t)0xff;

    assert(ptr1 == NULL); // Assertion fails.
    assert(ptr2 == NULL); // Assertion fails.
    assert(ptr3 == NULL); // Assertion fails.

    // Pointer fields in a local structure that simulate uninitialized state.
    struct foo f = {(int *)(uintptr_t)0xff, (int *)(uintptr_t)0xff};

    assert(f.a == NULL); // Assertion fails.
    assert(f.b == NULL); // Assertion fails.

    return 0;
}

int main() {
    gc_init();

    // Local pointer variables that simulate uninitialized state.
    int *ptr1 = (int *)(uintptr_t)0xff;
    int *ptr2 = (int *)(uintptr_t)0xff;
    int *ptr3 = (int *)(uintptr_t)0xff;
    gc_local_var(&ptr1);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);

    assert(ptr1 == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    // Pointer fields in a local structure that simulate uninitialized state.
    struct foo f = {(int *)(uintptr_t)0xff, (int *)(uintptr_t)0xff};
    gc_local_var(&f.a);
    gc_local_var(&f.b);

    assert(f.a == NULL);
    assert(f.b == NULL);

    gc_pop();

    gc_cleanup();

    return 0;
}
