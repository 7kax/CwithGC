#ifndef CWITHGC_COMMON_POINTER_TABLE_HPP
#define CWITHGC_COMMON_POINTER_TABLE_HPP

#include "gc.h"

#include <cstddef>
#include <vector>

struct gc_ptr_table {
    std::size_t array_len;
    std::size_t struct_size;
    std::vector<std::size_t> positions;
};

#endif // CWITHGC_COMMON_POINTER_TABLE_HPP
