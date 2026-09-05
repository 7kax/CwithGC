#include "gc.h"

#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>

void *operator new(std::size_t) {
    throw std::bad_alloc();
}

void operator delete(void *) noexcept {}

void operator delete(void *, std::size_t) noexcept {}

static void handle_abort(int signal_number) {
    if (signal_number == SIGABRT)
        std::_Exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;

    std::signal(SIGABRT, handle_abort);

    if (std::string_view(argv[1]) == "pointer-table") {
        const std::size_t positions[] = {0};
        (void)gc_ptr_table_create(1, sizeof(void *), 1, positions);
    } else if (std::string_view(argv[1]) == "root") {
        gc_init();
        void *root;
        gc_local_var(&root);
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
