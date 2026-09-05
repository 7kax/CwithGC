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

    (void)gc_malloc(SIZE_MAX);
    return EXIT_FAILURE;
}
