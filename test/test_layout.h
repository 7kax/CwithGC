#ifndef TEST_LAYOUT_H
#define TEST_LAYOUT_H

#include "gc.h"

#include <stddef.h>

static size_t test_gc_block_size(size_t payload_size) {
    const size_t alignment = _Alignof(max_align_t);
    const size_t unaligned_size = gc_meta_size() + payload_size;
    const size_t remainder = unaligned_size % alignment;
    return remainder == 0 ? unaligned_size : unaligned_size + alignment - remainder;
}

#endif // TEST_LAYOUT_H
