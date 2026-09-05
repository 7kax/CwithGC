#ifndef CWITHGC_COMMON_RUNTIME_HPP
#define CWITHGC_COMMON_RUNTIME_HPP

#include <cstdio>
#include <cstdlib>
#include <new>

namespace gc_runtime {

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
