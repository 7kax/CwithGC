#include "common/pointer_table.hpp"

#include "common/layout.hpp"
#include "common/runtime.hpp"

#include <cstddef>
#include <limits>
#include <utility>

namespace {

bool valid_pointer_offsets(std::size_t struct_size, std::size_t num_pointers,
                           const std::size_t *positions) noexcept {
    for (std::size_t i = 0; i < num_pointers; ++i) {
        if (positions[i] > struct_size - sizeof(void *) || positions[i] % alignof(void *) != 0)
            return false;
    }
    return true;
}

} // namespace

namespace gc_pointer_table {

bool valid_shape(std::size_t array_len, std::size_t struct_size, std::size_t num_pointers,
                 const std::size_t *positions) noexcept {
    if (array_len == 0 || struct_size == 0 || num_pointers == 0 || positions == nullptr ||
        num_pointers > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                           sizeof(std::size_t) ||
        !gc_layout::multiplication_fits(array_len, struct_size) || struct_size < sizeof(void *) ||
        (array_len > 1 && struct_size % alignof(void *) != 0))
        return false;

    return valid_pointer_offsets(struct_size, num_pointers, positions);
}

bool valid_for_payload(const gc_ptr_table &table, std::size_t payload_capacity) noexcept {
    if (!valid_shape(table.array_len, table.struct_size, table.positions.size(),
                     table.positions.data()))
        return false;

    std::size_t payload_size;
    return gc_layout::checked_mul(table.array_len, table.struct_size, payload_size) &&
           payload_size <= payload_capacity;
}

} // namespace gc_pointer_table

extern "C" {

gc_ptr_table *gc_ptr_table_create(size_t array_len, size_t struct_size, size_t num_pointers,
                                  const size_t *positions) noexcept try {
    if (!gc_pointer_table::valid_shape(array_len, struct_size, num_pointers, positions))
        gc_runtime::invalid_pointer_table();

    std::vector<std::size_t> copied_positions;
    copied_positions.reserve(num_pointers);
    for (std::size_t i = 0; i < num_pointers; ++i)
        copied_positions.push_back(positions[i]);

    return new gc_ptr_table{array_len, struct_size, std::move(copied_positions)};
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_ptr_table_destroy(gc_ptr_table *table) noexcept {
    delete table;
}

} // extern "C"
