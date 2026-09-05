#include "gc.h"

#include <signal.h>
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

    if (strcmp(argv[1], "malloc") == 0) {
        (void)gc_malloc(1);
    } else if (strcmp(argv[1], "collect") == 0) {
        gc_collect();
    } else if (strcmp(argv[1], "after-cleanup") == 0) {
        gc_init();
        gc_cleanup();
        (void)gc_malloc(1);
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
