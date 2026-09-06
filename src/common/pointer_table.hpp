#ifndef CWITHGC_COMMON_POINTER_TABLE_HPP
#define CWITHGC_COMMON_POINTER_TABLE_HPP

#include "gc.h"

#include <cstddef>
#include <vector>

struct gc_ptr_table {
    std::size_t array_len;
    std::size_t struct_size;
    std::vector<std::size_t> field_offsets;
};

namespace gc_pointer_table {

bool valid_shape(std::size_t array_len, std::size_t struct_size, std::size_t num_pointers,
                 const std::size_t *pointer_field_offsets) noexcept;

bool valid_for_payload(const gc_ptr_table &table, std::size_t payload_capacity) noexcept;

template <typename Visitor>
void for_each_field(const gc_ptr_table &table, void *payload, Visitor &&visitor) noexcept {
    auto *current = static_cast<std::byte *>(payload);
    for (std::size_t i = 0; i < table.array_len; ++i) {
        for (std::size_t field_offset : table.field_offsets)
            visitor(reinterpret_cast<void **>(current + field_offset));
        current += table.struct_size;
    }
}

} // namespace gc_pointer_table

#endif // CWITHGC_COMMON_POINTER_TABLE_HPP
