#include "gc.h"

#include <stddef.h>
#include <stdlib.h>

int main(void) {
    /* This sequence represents compiler-emitted runtime instrumentation. */
    const size_t pointer_field_offsets[] = {0};
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(void *), 1, pointer_field_offsets);
    if (table == NULL)
        return EXIT_FAILURE;

    gc_init();
    gc_scope_token scope = gc_scope_begin();
    void *root = NULL;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_malloc(sizeof(void *)));
    gc_register_object(root, table);
    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(table);
    return EXIT_SUCCESS;
}
