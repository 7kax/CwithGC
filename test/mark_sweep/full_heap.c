#include "../test_debug.h"
#include "../test_types.h"
#include "gc.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    unsigned char *root;
    gc_scope_add_root(&root);

    const size_t payload_size = test_gc_heap_capacity() - test_gc_metadata_size();
    gc_pointer_assign(&root, gc_alloc_array(test_gc_byte_type(), payload_size));
    memset(root, 0x5a, payload_size);
    TEST_CHECK(test_gc_free_bytes() == 0);

    // An empty free-list is valid while the entire heap remains reachable.
    gc_collect();
    TEST_CHECK(root != NULL);
    TEST_CHECK(test_gc_free_bytes() == 0);
    TEST_CHECK(test_gc_reclaimed_block_count() == 0);
    TEST_CHECK(((unsigned char *)root)[0] == 0x5a);
    TEST_CHECK(((unsigned char *)root)[payload_size - 1] == 0x5a);

    // Once the object becomes unreachable, sweep must rebuild the free-list.
    gc_pointer_assign(&root, NULL);
    gc_collect();
    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());
    TEST_CHECK(test_gc_reclaimed_block_count() == 1);

    // The rebuilt free-list must support another full-heap allocation.
    gc_pointer_assign(&root, gc_alloc_array(test_gc_byte_type(), payload_size));
    TEST_CHECK(root != NULL);
    TEST_CHECK(test_gc_free_bytes() == 0);

    gc_scope_end(scope);
    gc_cleanup();

    puts("Mark-sweep full heap test passed!");
    return 0;
}
