#ifndef GC_PTR_TABLE_INTERNAL_H
#define GC_PTR_TABLE_INTERNAL_H

#include "gc.h"

#include <cstddef>
#include <vector>

struct gc_ptr_table {
    std::size_t array_len;
    std::size_t struct_size;
    std::vector<std::size_t> positions;
};

#endif // GC_PTR_TABLE_INTERNAL_H
