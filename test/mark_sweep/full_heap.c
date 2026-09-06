#include "../test_debug.h"
#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    gc_init();
    gc_scope_token scope = gc_scope_begin();

    void *root;
    gc_scope_add_root(&root);

    const size_t payload_size = test_gc_heap_capacity() - test_gc_metadata_size();
    gc_pointer_assign(&root, gc_malloc(payload_size));
    memset(root, 0x5a, payload_size);
    assert(test_gc_free_bytes() == 0);

    // An empty free-list is valid while the entire heap remains reachable.
    gc_collect();
    assert(root != NULL);
    assert(test_gc_free_bytes() == 0);
    assert(test_gc_reclaimed_block_count() == 0);
    assert(((unsigned char *)root)[0] == 0x5a);
    assert(((unsigned char *)root)[payload_size - 1] == 0x5a);

    // Once the object becomes unreachable, sweep must rebuild the free-list.
    gc_pointer_assign(&root, NULL);
    gc_collect();
    assert(test_gc_free_bytes() == test_gc_heap_capacity());
    assert(test_gc_reclaimed_block_count() == 1);

    // The rebuilt free-list must support another full-heap allocation.
    gc_pointer_assign(&root, gc_malloc(payload_size));
    assert(root != NULL);
    assert(test_gc_free_bytes() == 0);

    gc_scope_end(scope);
    gc_cleanup();

    puts("Mark-sweep full heap test passed!");
    return 0;
}
