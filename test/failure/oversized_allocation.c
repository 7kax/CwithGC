#include "gc.h"

#include <stdint.h>
#include <stdlib.h>

int main(void) {
    gc_init();

    (void)gc_malloc(SIZE_MAX);
    return EXIT_FAILURE;
}
