#include "gc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    size_t table_size;
    assert(gc_ptr_table_size(2, &table_size));
    assert(table_size == offsetof(gc_ptr_table, positions) + 2 * sizeof(size_t));

    const size_t max_pointers = (SIZE_MAX - offsetof(gc_ptr_table, positions)) / sizeof(size_t);
    assert(!gc_ptr_table_size(max_pointers + 1, &table_size));
    assert(!gc_ptr_table_size(1, NULL));

    puts("Pointer table size test passed!");
    return 0;
}
