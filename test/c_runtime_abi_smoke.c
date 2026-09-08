#include "gc.h"

#include <stddef.h>
#include <stdlib.h>

static const size_t pointer_field_offsets[] = {0};
static const gc_type_descriptor pointer_type = {
    sizeof(void *),
    sizeof(pointer_field_offsets) / sizeof(pointer_field_offsets[0]),
    pointer_field_offsets,
};

int main(void) {
    /* This sequence represents compiler-emitted runtime instrumentation. */
    gc_init();
    gc_scope_token scope = gc_scope_begin();
    void **root = NULL;
    void **array_root = NULL;
    gc_scope_add_root(&root);
    gc_scope_add_root(&array_root);
    gc_pointer_assign(&root, gc_alloc_object(&pointer_type));
    gc_pointer_assign(&array_root, gc_alloc_array(&pointer_type, 1));
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    return EXIT_SUCCESS;
}
