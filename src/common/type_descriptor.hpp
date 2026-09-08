#ifndef CWITHGC_COMMON_TYPE_DESCRIPTOR_HPP
#define CWITHGC_COMMON_TYPE_DESCRIPTOR_HPP

#include "common/layout.hpp"
#include "common/runtime.hpp"
#include "gc.h"

#include <cstddef>

namespace gc_type_layout {

inline std::size_t payload_size(const ::gc_type_descriptor *type,
                                std::size_t element_count) noexcept {
    if (type == nullptr || type->element_size == 0 ||
        (type->pointer_count != 0 && type->pointer_offsets == nullptr))
        gc_runtime::invalid_type_descriptor();

    std::size_t result;
    if (element_count == 0 || !gc_layout::checked_mul(type->element_size, element_count, result))
        gc_runtime::allocation_failure();

    return result;
}

template <typename Visitor>
void for_each_pointer(const ::gc_type_descriptor &type, std::size_t element_count, void *payload,
                      Visitor &&visitor) noexcept {
    auto *element = static_cast<std::byte *>(payload);
    for (std::size_t index = 0; index < element_count; ++index) {
        for (std::size_t pointer_index = 0; pointer_index < type.pointer_count; ++pointer_index)
            visitor(static_cast<void *>(element + type.pointer_offsets[pointer_index]));
        element += type.element_size;
    }
}

} // namespace gc_type_layout

#endif // CWITHGC_COMMON_TYPE_DESCRIPTOR_HPP
