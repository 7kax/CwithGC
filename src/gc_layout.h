#ifndef GC_LAYOUT_H
#define GC_LAYOUT_H

#include <cstddef>
#include <limits>

namespace gc_layout {

inline constexpr std::size_t alignment = alignof(std::max_align_t);

constexpr std::size_t align_up(std::size_t size) {
    const std::size_t remainder = size % alignment;
    return remainder == 0 ? size : size + alignment - remainder;
}

template <typename Metadata> inline constexpr std::size_t header_size = align_up(sizeof(Metadata));

template <typename Metadata> bool block_size(std::size_t payload_size, std::size_t &result) {
    constexpr std::size_t header = header_size<Metadata>;
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();

    if (payload_size > max_size - header)
        return false;

    const std::size_t unaligned_size = header + payload_size;
    const std::size_t remainder = unaligned_size % alignment;
    const std::size_t padding = remainder == 0 ? 0 : alignment - remainder;
    if (unaligned_size > max_size - padding)
        return false;

    result = unaligned_size + padding;
    return true;
}

template <typename Metadata> void *payload(Metadata *metadata) {
    return reinterpret_cast<std::byte *>(metadata) + header_size<Metadata>;
}

template <typename Metadata> Metadata *metadata(void *payload) {
    return reinterpret_cast<Metadata *>(static_cast<std::byte *>(payload) - header_size<Metadata>);
}

} // namespace gc_layout

#endif // GC_LAYOUT_H
