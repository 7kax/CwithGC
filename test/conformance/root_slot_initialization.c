#include "gc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct pointer_pair {
    int *first;
    int *second;
};

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    // Nonzero placeholders model storage before instrumentation registers it.
    int *ptr1 = (int *)(uintptr_t)0xff;
    int *ptr2 = (int *)(uintptr_t)0xff;
    int *ptr3 = (int *)(uintptr_t)0xff;
    gc_scope_add_root(&ptr1);
    gc_scope_add_root(&ptr2);
    gc_scope_add_root(&ptr3);

    assert(ptr1 == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    // The same initialization contract applies to pointer slots inside locals.
    struct pointer_pair pair = {(int *)(uintptr_t)0xff, (int *)(uintptr_t)0xff};
    gc_scope_add_root(&pair.first);
    gc_scope_add_root(&pair.second);

    assert(pair.first == NULL);
    assert(pair.second == NULL);

    gc_scope_end(scope);

    gc_cleanup();

    return 0;
}
