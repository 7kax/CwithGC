#include "gc.h"

#include <cstddef>
#include <cstdlib>

static_assert(noexcept(gc_ptr_table_create(0, 0, 0, nullptr)));
static_assert(noexcept(gc_ptr_table_destroy(nullptr)));
static_assert(noexcept(gc_init()));
static_assert(noexcept(gc_scope_begin()));
static_assert(noexcept(gc_scope_end(0)));
static_assert(noexcept(gc_malloc(0)));
static_assert(noexcept(gc_local_var(nullptr)));
static_assert(noexcept(gc_register(nullptr, nullptr)));
static_assert(noexcept(gc_ptr_copy(nullptr, nullptr)));
static_assert(noexcept(gc_collect()));
static_assert(noexcept(gc_pop()));
static_assert(noexcept(gc_cleanup()));
static_assert(noexcept(gc_allocation_failure()));

int main() {
    const std::size_t positions[] = {0};
    auto *table = gc_ptr_table_create(1, sizeof(void *), 1, positions);
    if (table == nullptr)
        return EXIT_FAILURE;

    gc_init();
    const gc_scope_token scope = gc_scope_begin();
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(table);
    return EXIT_SUCCESS;
}
