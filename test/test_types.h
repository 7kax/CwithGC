#ifndef TEST_TYPES_H
#define TEST_TYPES_H

#include "gc.h"

#include <stddef.h>

static inline const gc_type_descriptor *test_gc_byte_type(void) {
    static const gc_type_descriptor type = {sizeof(unsigned char), 0, NULL};
    return &type;
}

static inline const gc_type_descriptor *test_gc_int_type(void) {
    static const gc_type_descriptor type = {sizeof(int), 0, NULL};
    return &type;
}

static inline const gc_type_descriptor *test_gc_max_align_type(void) {
    static const gc_type_descriptor type = {sizeof(max_align_t), 0, NULL};
    return &type;
}

#endif // TEST_TYPES_H
