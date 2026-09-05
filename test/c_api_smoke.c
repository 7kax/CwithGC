#include "gc.h"

#include <stddef.h>
#include <stdlib.h>

int main(void) {
    const size_t positions[] = {0};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(void *), 1, positions);
    if (table == NULL)
        return EXIT_FAILURE;

    gc_init();
    gc_collect();
    gc_cleanup();
    gc_ptr_table_destroy(table);
    return EXIT_SUCCESS;
}
