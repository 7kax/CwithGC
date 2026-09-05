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

    const size_t positions[] = {0};
    if (strcmp(argv[1], "overflow") == 0) {
        (void)gc_ptr_table_create(SIZE_MAX, 2, 1, positions);
    } else if (strcmp(argv[1], "zero-array") == 0) {
        (void)gc_ptr_table_create(0, sizeof(void *), 1, positions);
    } else if (strcmp(argv[1], "bad-offset") == 0) {
        const size_t bad_positions[] = {sizeof(void *)};
        (void)gc_ptr_table_create(1, sizeof(void *), 1, bad_positions);
    } else if (strcmp(argv[1], "bad-stride") == 0) {
        (void)gc_ptr_table_create(2, sizeof(void *) + 1, 1, positions);
    } else if (strcmp(argv[1], "large-payload") == 0) {
        gc_ptr_table *table = gc_ptr_table_create(3, sizeof(void *), 1, positions);

        gc_init();
        void *root;
        gc_local_var(&root);
        gc_ptr_copy(&root, gc_malloc(sizeof(void *)));
        gc_register(root, table);
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
