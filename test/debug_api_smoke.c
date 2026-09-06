#include "gc_debug.h"

#include <assert.h>
#include <stddef.h>

#ifndef TEST_EXPECT_INSPECTION
#error "TEST_EXPECT_INSPECTION must be defined by the test target"
#endif

static void assert_zero_stats(const gc_debug_stats *stats) {
    assert(stats->heap_capacity == 0);
    assert(stats->free_bytes == 0);
    assert(stats->reclaimed_block_count == 0);
    assert(stats->relocated_block_count == 0);
    assert(stats->metadata_size == 0);
    assert(stats->root_count == 0);
}

static void assert_zero_layout(const gc_debug_memory_layout *layout) {
    assert(layout->blocks == NULL);
    assert(layout->block_count == 0);
}

int main(void) {
    assert(gc_debug_get_stats(NULL) == GC_DEBUG_INVALID_ARGUMENT);
    assert(gc_debug_snapshot_memory_layout(NULL) == GC_DEBUG_INVALID_ARGUMENT);

#if TEST_EXPECT_INSPECTION
    assert(gc_debug_is_available() == 1);

    gc_debug_stats stats = {1, 2, 3, 4, 5, 6};
    assert(gc_debug_get_stats(&stats) == GC_DEBUG_NOT_INITIALIZED);
    assert_zero_stats(&stats);

    gc_debug_memory_layout layout = {(gc_debug_memory_block *)1, 1};
    assert(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_NOT_INITIALIZED);
    assert_zero_layout(&layout);

    gc_init();
    assert(gc_debug_get_stats(&stats) == GC_DEBUG_OK);
    assert(stats.heap_capacity > 0);
    assert(stats.free_bytes == stats.heap_capacity);
    assert(stats.metadata_size > 0);
    assert(stats.reclaimed_block_count == 0);
    assert(stats.relocated_block_count == 0);
    assert(stats.root_count == 0);

    gc_scope_token scope = gc_scope_begin();
    void *root;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_malloc(1));

    assert(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_OK);
    assert(layout.block_count > 0);
    assert(layout.blocks != NULL);
    for (size_t i = 0; i < layout.block_count; ++i) {
        assert(layout.blocks[i].start != NULL);
        assert(layout.blocks[i].size > 0);
        assert(layout.blocks[i].state == GC_DEBUG_BLOCK_ALLOCATED ||
               layout.blocks[i].state == GC_DEBUG_BLOCK_FREE);
    }
    gc_debug_memory_layout_dispose(&layout);
    assert_zero_layout(&layout);

    gc_pointer_assign(&root, NULL);
    gc_scope_end(scope);
    gc_cleanup();
#else
    assert(gc_debug_is_available() == 0);

    gc_debug_stats stats = {1, 2, 3, 4, 5, 6};
    assert(gc_debug_get_stats(&stats) == GC_DEBUG_UNAVAILABLE);
    assert_zero_stats(&stats);

    gc_debug_memory_layout layout = {(gc_debug_memory_block *)1, 1};
    assert(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_UNAVAILABLE);
    assert_zero_layout(&layout);
    gc_debug_memory_layout_dispose(&layout);
#endif

    return 0;
}
