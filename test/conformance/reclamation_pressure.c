#include "../test_check.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct pressure_object {
    unsigned char bytes[128];
};

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    struct pressure_object *slot;
    gc_scope_add_root(&slot);

    // All current collectors use a fixed 4 KiB allocation budget. The payloads
    // in this sequence alone exceed that budget. Every object is dropped before
    // collection, so the loop only completes when the current implementations
    // reclaim unreachable allocations.
    for (size_t iteration = 0; iteration < 64; iteration++) {
        gc_pointer_assign(&slot, gc_malloc(sizeof(*slot)));
        TEST_CHECK(slot != NULL);
        memset(slot->bytes, (int)(iteration & 0xff), sizeof(slot->bytes));
        gc_pointer_assign(&slot, NULL);
        gc_collect();
    }

    gc_pointer_assign(&slot, gc_malloc(sizeof(*slot)));
    TEST_CHECK(slot != NULL);
    memset(slot->bytes, 0xa5, sizeof(slot->bytes));

    for (size_t round = 0; round < 4; round++) {
        gc_collect();
        TEST_CHECK(slot != NULL);
        for (size_t i = 0; i < sizeof(slot->bytes); i++)
            TEST_CHECK(slot->bytes[i] == 0xa5);
    }

    gc_pointer_assign(&slot, NULL);
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();

    puts("Reclamation pressure test passed!");
    return 0;
}
