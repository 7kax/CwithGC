#include "../test_debug.h"
#include "../test_types.h"
#include "gc.h"

#include <stdlib.h>

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    unsigned char *root;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_alloc_array(test_gc_byte_type(),
                                            test_gc_heap_capacity() - test_gc_metadata_size()));
    TEST_CHECK(test_gc_free_bytes() == 0);

    // Collection cannot reclaim the live full-heap object, so this request
    // must reach the runtime allocation-failure path instead of an assertion.
    (void)gc_alloc_object(test_gc_byte_type());

    gc_scope_end(scope);
    gc_cleanup();

    return EXIT_FAILURE;
}
