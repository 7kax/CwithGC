#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    gc_init();

    void *root;
    gc_local_var(&root);

    const size_t payload_size = gc_heap_size() - gc_meta_size();
    gc_ptr_copy(&root, gc_malloc(payload_size));
    memset(root, 0x5a, payload_size);
    assert(gc_free_size() == 0);

    // An empty free-list is valid while the entire heap remains reachable.
    gc_collect();
    assert(root != NULL);
    assert(gc_free_size() == 0);
    assert(gc_block_collected() == 0);
    assert(((unsigned char *)root)[0] == 0x5a);
    assert(((unsigned char *)root)[payload_size - 1] == 0x5a);

    // Once the object becomes unreachable, sweep must rebuild the free-list.
    gc_ptr_copy(&root, NULL);
    gc_collect();
    assert(gc_free_size() == gc_heap_size());
    assert(gc_block_collected() == 1);

    // The rebuilt free-list must support another full-heap allocation.
    gc_ptr_copy(&root, gc_malloc(payload_size));
    assert(root != NULL);
    assert(gc_free_size() == 0);

    gc_pop();
    gc_cleanup();

    puts("Mark-sweep full heap test passed!");
    return 0;
}
