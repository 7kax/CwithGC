#include "gc.h"

#include <stddef.h>
#include <stdlib.h>

int main(void) {
    static const gc_type_descriptor byte_type = {sizeof(unsigned char), 0, NULL};

    gc_init();
    (void)gc_alloc_array(&byte_type, 0);
    return EXIT_FAILURE;
}
