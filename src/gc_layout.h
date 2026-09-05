#ifndef GC_LAYOUT_H
#define GC_LAYOUT_H

#include <cstddef>
#include <limits>

namespace gc_layout {

inline constexpr std::size_t alignment = alignof(std::max_align_t);

constexpr bool checked_add(std::size_t lhs, std::size_t rhs, std::size_t &result) {
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (lhs > max_size - rhs)
        return false;

    result = lhs + rhs;
    return true;
}

constexpr bool checked_mul(std::size_t lhs, std::size_t rhs, std::size_t &result) {
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (lhs != 0 && rhs > max_size / lhs)
        return false;

    result = lhs * rhs;
    return true;
}

constexpr std::size_t align_up(std::size_t size) {
    const std::size_t remainder = size % alignment;
    return remainder == 0 ? size : size + alignment - remainder;
}

template <typename Metadata> inline constexpr std::size_t header_size = align_up(sizeof(Metadata));

template <typename Metadata> bool block_size(std::size_t payload_size, std::size_t &result) {
    constexpr std::size_t header = header_size<Metadata>;

    std::size_t unaligned_size;
    if (!checked_add(header, payload_size, unaligned_size))
        return false;

    const std::size_t remainder = unaligned_size % alignment;
    const std::size_t padding = remainder == 0 ? 0 : alignment - remainder;
    return checked_add(unaligned_size, padding, result);
}

template <typename Metadata> void *payload(Metadata *metadata) {
    return reinterpret_cast<std::byte *>(metadata) + header_size<Metadata>;
}

template <typename Metadata> Metadata *metadata(void *payload) {
    return reinterpret_cast<Metadata *>(static_cast<std::byte *>(payload) - header_size<Metadata>);
}

} // namespace gc_layout

#endif // GC_LAYOUT_H
