#ifndef CWITHGC_COMMON_RUNTIME_HPP
#define CWITHGC_COMMON_RUNTIME_HPP

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>

namespace gc_runtime {

struct free_deleter {
    void operator()(void *ptr) const noexcept { std::free(ptr); }
};

template <typename T = std::byte> using malloc_ptr = std::unique_ptr<T, free_deleter>;

template <typename T = std::byte> malloc_ptr<T> malloc_bytes(std::size_t size) {
    void *memory = std::malloc(size);
    if (memory == nullptr)
        throw std::bad_alloc();

    return malloc_ptr<T>(static_cast<T *>(memory));
}

[[noreturn]] inline void fatal(const char *message) noexcept {
    std::fputs(message, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

[[noreturn]] inline void handle_current_exception() noexcept {
    try {
        throw;
    } catch (const std::bad_alloc &) {
        fatal("Allocation failure");
    } catch (...) {
        fatal("Internal GC failure");
    }
}

} // namespace gc_runtime

#endif // CWITHGC_COMMON_RUNTIME_HPP
