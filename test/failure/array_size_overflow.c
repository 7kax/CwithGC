#include "gc.h"

#include <stdint.h>
#include <stdlib.h>

int main(void) {
    static const gc_type_descriptor two_byte_type = {2, 0, NULL};

    gc_init();
    (void)gc_alloc_array(&two_byte_type, SIZE_MAX);
    return EXIT_FAILURE;
}
