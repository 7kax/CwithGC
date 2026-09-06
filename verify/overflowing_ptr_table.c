#include "gc.h"

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void handle_abort(int signal_number) {
    if (signal_number == SIGABRT)
        _Exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;

    signal(SIGABRT, handle_abort);

    const size_t pointer_field_offsets[] = {0};
    if (strcmp(argv[1], "overflow") == 0) {
        (void)gc_ptr_table_create(SIZE_MAX, 2, 1, pointer_field_offsets);
    } else if (strcmp(argv[1], "zero-array") == 0) {
        (void)gc_ptr_table_create(0, sizeof(void *), 1, pointer_field_offsets);
    } else if (strcmp(argv[1], "bad-offset") == 0) {
        const size_t invalid_offsets[] = {sizeof(void *)};
        (void)gc_ptr_table_create(1, sizeof(void *), 1, invalid_offsets);
    } else if (strcmp(argv[1], "bad-stride") == 0) {
        (void)gc_ptr_table_create(2, sizeof(void *) + 1, 1, pointer_field_offsets);
    } else if (strcmp(argv[1], "large-payload") == 0) {
        gc_ptr_table *table = gc_ptr_table_create(3, sizeof(void *), 1, pointer_field_offsets);

        gc_init();
        gc_scope_token scope = gc_scope_begin();
        void *root;
        gc_scope_add_root(&root);
        gc_pointer_assign(&root, gc_malloc(sizeof(void *)));
        gc_register_object(root, table);
        gc_scope_end(scope);
        gc_cleanup();
        gc_ptr_table_destroy(table);
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
