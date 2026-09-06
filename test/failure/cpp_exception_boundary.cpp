#include "gc.h"

#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>

void *operator new(std::size_t) {
    throw std::bad_alloc();
}

void operator delete(void *) noexcept {}

void operator delete(void *, std::size_t) noexcept {}

int main(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;

    if (std::string_view(argv[1]) == "pointer-table") {
        const std::size_t pointer_field_offsets[] = {0};
        (void)gc_ptr_table_create(1, sizeof(void *), 1, pointer_field_offsets);
    } else if (std::string_view(argv[1]) == "root") {
        gc_init();
        const gc_scope_token scope = gc_scope_begin();
        void *root;
        gc_scope_add_root(&root);
        gc_scope_end(scope);
        gc_cleanup();
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
