#include "../test_check.h"
#include "../test_types.h"
#include "gc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const size_t sizes[] = {1, 3, 7, sizeof(long double), sizeof(max_align_t), 33};
    const size_t count = sizeof(sizes) / sizeof(sizes[0]);
    const size_t alignment = _Alignof(max_align_t);
    unsigned char *roots[sizeof(sizes) / sizeof(sizes[0])];
    max_align_t *aligned_root;

    gc_init();
    gc_scope_token scope = gc_scope_begin();
    gc_scope_add_root(&aligned_root);

    for (size_t i = 0; i < count; i++) {
        gc_scope_add_root(&roots[i]);
        gc_pointer_assign(&roots[i], gc_alloc_array(test_gc_byte_type(), sizes[i]));
        TEST_CHECK((uintptr_t)roots[i] % alignment == 0);

        memset(roots[i], (int)(i + 1), sizes[i]);
    }

    gc_collect();

    for (size_t i = 0; i < count; i++) {
        TEST_CHECK((uintptr_t)roots[i] % alignment == 0);
        for (size_t j = 0; j < sizes[i]; j++)
            TEST_CHECK(((unsigned char *)roots[i])[j] == (unsigned char)(i + 1));

        gc_pointer_assign(&roots[i], NULL);
    }

    gc_collect();

    gc_pointer_assign(&aligned_root, gc_alloc_object(test_gc_max_align_type()));
    TEST_CHECK((uintptr_t)aligned_root % alignment == 0);
    *aligned_root = (max_align_t){0};
    memset(aligned_root, 0x5a, sizeof(max_align_t));
    gc_collect();
    for (size_t i = 0; i < sizeof(max_align_t); i++)
        TEST_CHECK(((unsigned char *)aligned_root)[i] == 0x5a);
    gc_pointer_assign(&aligned_root, NULL);

    gc_scope_end(scope);
    gc_cleanup();

    puts("GC alignment test passed!");
    return 0;
}
