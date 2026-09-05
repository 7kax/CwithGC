#include "gc.h"

#include <assert.h>
#include <stdio.h>

void func(void) {
    int *ptr;
    gc_local_var(&ptr);

    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    gc_pop();
}

int main(void) {
    gc_init();

    int prev_free_size = gc_free_size();

    func();

    // Trigger garbage collection.
    gc_collect();

    // Calculate the leaked size.
    int curr_free_size = gc_free_size();
    int leak_size = prev_free_size - curr_free_size;

    // Assert that no memory was leaked.
    assert(leak_size == 0);

    gc_pop();

    gc_cleanup();

    return 0;
}
