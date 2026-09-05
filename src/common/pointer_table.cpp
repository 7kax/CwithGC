#include "common/pointer_table.hpp"

#include "common/layout.hpp"
#include "common/runtime.hpp"

#include <cstddef>
#include <limits>

namespace {

[[noreturn]] void invalid_pointer_table() {
    gc_runtime::fatal("Invalid pointer table");
}

} // namespace

extern "C" {

gc_ptr_table *gc_ptr_table_create(size_t array_len, size_t struct_size, size_t num_pointers,
                                  const size_t *positions) noexcept try {
    std::size_t payload_size;
    if (array_len == 0 || struct_size == 0 || num_pointers == 0 || positions == nullptr ||
        num_pointers > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                           sizeof(std::size_t) ||
        !gc_layout::checked_mul(array_len, struct_size, payload_size) ||
        struct_size < sizeof(void *) || (array_len > 1 && struct_size % alignof(void *) != 0))
        invalid_pointer_table();

    for (std::size_t i = 0; i < num_pointers; ++i) {
        if (positions[i] > struct_size - sizeof(void *) || positions[i] % alignof(void *) != 0)
            invalid_pointer_table();
    }

    return new gc_ptr_table{array_len, struct_size,
                            std::vector<std::size_t>(positions, positions + num_pointers)};
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_ptr_table_destroy(gc_ptr_table *table) noexcept {
    delete table;
}

} // extern "C"
