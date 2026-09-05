#include "gc.h"

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

static void handle_abort(int signal_number) {
    if (signal_number == SIGABRT)
        _Exit(EXIT_SUCCESS);
}

int main(void) {
    signal(SIGABRT, handle_abort);
    gc_init();

    void *root;
    gc_local_var(&root);
    gc_ptr_copy(&root, gc_malloc(sizeof(void *)));

    size_t table_size;
    if (!gc_ptr_table_size(1, &table_size))
        return EXIT_FAILURE;

    gc_ptr_table *table = malloc(table_size);
    if (table == NULL)
        return EXIT_FAILURE;

    table->array_len = SIZE_MAX;
    table->struct_size = 2;
    table->num_pointers = 1;
    table->positions[0] = 0;

    gc_register(root, table);
    return EXIT_FAILURE;
}
