#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const size_t sizes[] = {1, 3, 7, sizeof(long double), sizeof(max_align_t), 33};
    const size_t count = sizeof(sizes) / sizeof(sizes[0]);
    const size_t alignment = _Alignof(max_align_t);
    void *roots[sizeof(sizes) / sizeof(sizes[0])];

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    for (size_t i = 0; i < count; i++) {
        gc_scope_add_root(&roots[i]);
        gc_pointer_assign(&roots[i], gc_malloc(sizes[i]));
        assert((uintptr_t)roots[i] % alignment == 0);

        if (sizes[i] == sizeof(max_align_t))
            *(max_align_t *)roots[i] = (max_align_t){0};
        memset(roots[i], (int)(i + 1), sizes[i]);
    }

    gc_collect();

    for (size_t i = 0; i < count; i++) {
        assert((uintptr_t)roots[i] % alignment == 0);
        for (size_t j = 0; j < sizes[i]; j++)
            assert(((unsigned char *)roots[i])[j] == (unsigned char)(i + 1));

        if (sizes[i] == sizeof(max_align_t))
            *(max_align_t *)roots[i] = (max_align_t){0};
        gc_pointer_assign(&roots[i], NULL);
    }

    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_pointer_assign(&roots[0], gc_malloc(sizeof(max_align_t)));
    assert((uintptr_t)roots[0] % alignment == 0);
    *(max_align_t *)roots[0] = (max_align_t){0};
    gc_pointer_assign(&roots[0], NULL);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());

    gc_scope_end(scope);
    gc_cleanup();

    puts("GC alignment test passed!");
    return 0;
}
