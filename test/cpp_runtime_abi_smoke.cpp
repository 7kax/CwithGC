#include "gc.h"

#include <cstddef>
#include <cstdlib>

static_assert(noexcept(gc_ptr_table_create(0, 0, 0, nullptr)));
static_assert(noexcept(gc_ptr_table_destroy(nullptr)));
static_assert(noexcept(gc_init()));
static_assert(noexcept(gc_scope_begin()));
static_assert(noexcept(gc_scope_end(0)));
static_assert(noexcept(gc_malloc(0)));
static_assert(noexcept(gc_scope_add_root(nullptr)));
static_assert(noexcept(gc_register_object(nullptr, nullptr)));
static_assert(noexcept(gc_pointer_assign(nullptr, nullptr)));
static_assert(noexcept(gc_collect()));
static_assert(noexcept(gc_cleanup()));

int main() {
    // This sequence represents compiler-emitted runtime instrumentation.
    const std::size_t pointer_field_offsets[] = {0};
    auto *table = gc_ptr_table_create(1, sizeof(void *), 1, pointer_field_offsets);
    if (table == nullptr)
        return EXIT_FAILURE;

    gc_init();
    const gc_scope_token scope = gc_scope_begin();
    void *root = nullptr;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_malloc(sizeof(void *)));
    gc_register_object(root, table);
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(table);
    return EXIT_SUCCESS;
}
