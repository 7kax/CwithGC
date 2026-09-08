#ifndef CWITHGC_COMMON_RUNTIME_HPP
#define CWITHGC_COMMON_RUNTIME_HPP

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

namespace gc_runtime {

struct FreeDeleter {
    void operator()(void *ptr) const noexcept { std::free(ptr); }
};

template <typename T = std::byte> using MallocPtr = std::unique_ptr<T, FreeDeleter>;

// Managed slots may have typed pointer storage. Byte-wise access preserves the
// slot's declared type while honoring the ABI v2 representation invariant.
inline void *load_pointer(const void *slot) noexcept {
    void *value;
    std::memcpy(&value, slot, sizeof(value));
    return value;
}

inline void store_pointer(void *slot, void *value) noexcept {
    std::memcpy(slot, &value, sizeof(value));
}

template <typename T = std::byte> MallocPtr<T> malloc_bytes(std::size_t size) {
    void *memory = std::malloc(size);
    if (memory == nullptr)
        throw std::bad_alloc();

    return MallocPtr<T>(static_cast<T *>(memory));
}

[[noreturn]] inline void fatal(const char *message) noexcept {
    std::fputs(message, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

[[noreturn]] inline void allocation_failure(const char *message = "Allocation failure") noexcept {
    fatal(message);
}

[[noreturn]] inline void invalid_type_descriptor() noexcept {
    fatal("Invalid type descriptor");
}

inline void require_initialized(bool initialized) noexcept {
    if (!initialized)
        fatal("GC is not initialized");
}

[[noreturn]] inline void handle_current_exception() noexcept {
    try {
        throw;
    } catch (const std::bad_alloc &) {
        allocation_failure();
    } catch (...) {
        fatal("Internal GC failure");
    }
}

} // namespace gc_runtime

#endif // CWITHGC_COMMON_RUNTIME_HPP
