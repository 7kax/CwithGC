#include "gc.h"

#include <cstddef>
#include <cstdlib>

static_assert(noexcept(gc_init()));
static_assert(noexcept(gc_scope_begin()));
static_assert(noexcept(gc_scope_end(0)));
static_assert(noexcept(gc_alloc_object(nullptr)));
static_assert(noexcept(gc_alloc_array(nullptr, 0)));
static_assert(noexcept(gc_scope_add_root(nullptr)));
static_assert(noexcept(gc_pointer_assign(nullptr, nullptr)));
static_assert(noexcept(gc_collect()));
static_assert(noexcept(gc_cleanup()));

int main() {
    // This sequence represents compiler-emitted runtime instrumentation.
    static constexpr std::size_t pointer_field_offsets[] = {0};
    static constexpr gc_type_descriptor pointer_type = {
        sizeof(void *),
        sizeof(pointer_field_offsets) / sizeof(pointer_field_offsets[0]),
        pointer_field_offsets,
    };

    gc_init();
    const gc_scope_token scope = gc_scope_begin();
    void **root = nullptr;
    void **array_root = nullptr;
    gc_scope_add_root(&root);
    gc_scope_add_root(&array_root);
    gc_pointer_assign(&root, gc_alloc_object(&pointer_type));
    gc_pointer_assign(&array_root, gc_alloc_array(&pointer_type, 1));
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    return EXIT_SUCCESS;
}
