#include "gc_debug.h"
#include "test_check.h"
#include "test_types.h"

#include <stddef.h>

#ifndef TEST_EXPECT_INSPECTION
#error "TEST_EXPECT_INSPECTION must be defined by the test target"
#endif

static void assert_zero_stats(const gc_debug_stats *stats) {
    TEST_CHECK(stats->heap_capacity == 0);
    TEST_CHECK(stats->free_bytes == 0);
    TEST_CHECK(stats->reclaimed_block_count == 0);
    TEST_CHECK(stats->relocated_block_count == 0);
    TEST_CHECK(stats->metadata_size == 0);
    TEST_CHECK(stats->root_count == 0);
}

static void assert_zero_layout(const gc_debug_memory_layout *layout) {
    TEST_CHECK(layout->blocks == NULL);
    TEST_CHECK(layout->block_count == 0);
}

int main(void) {
    TEST_CHECK(gc_debug_get_stats(NULL) == GC_DEBUG_INVALID_ARGUMENT);
    TEST_CHECK(gc_debug_snapshot_memory_layout(NULL) == GC_DEBUG_INVALID_ARGUMENT);

#if TEST_EXPECT_INSPECTION
    TEST_CHECK(gc_debug_is_available() == 1);

    gc_debug_stats stats = {1, 2, 3, 4, 5, 6};
    TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_NOT_INITIALIZED);
    assert_zero_stats(&stats);

    gc_debug_memory_layout layout = {(gc_debug_memory_block *)1, 1};
    TEST_CHECK(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_NOT_INITIALIZED);
    assert_zero_layout(&layout);

    gc_init();
    TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_OK);
    TEST_CHECK(stats.heap_capacity > 0);
    TEST_CHECK(stats.free_bytes == stats.heap_capacity);
    TEST_CHECK(stats.metadata_size > 0);
    TEST_CHECK(stats.reclaimed_block_count == 0);
    TEST_CHECK(stats.relocated_block_count == 0);
    TEST_CHECK(stats.root_count == 0);

    gc_scope_token scope = gc_scope_begin();
    unsigned char *root;
    gc_scope_add_root(&root);
    gc_pointer_assign(&root, gc_alloc_object(test_gc_byte_type()));

    TEST_CHECK(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_OK);
    TEST_CHECK(layout.block_count > 0);
    TEST_CHECK(layout.blocks != NULL);
    for (size_t i = 0; i < layout.block_count; ++i) {
        TEST_CHECK(layout.blocks[i].start != NULL);
        TEST_CHECK(layout.blocks[i].size > 0);
        TEST_CHECK(layout.blocks[i].state == GC_DEBUG_BLOCK_ALLOCATED ||
                   layout.blocks[i].state == GC_DEBUG_BLOCK_FREE);
    }
    gc_debug_memory_layout_dispose(&layout);
    assert_zero_layout(&layout);

    gc_pointer_assign(&root, NULL);
    gc_scope_end(scope);
    gc_cleanup();
#else
    TEST_CHECK(gc_debug_is_available() == 0);

    gc_debug_stats stats = {1, 2, 3, 4, 5, 6};
    TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_UNAVAILABLE);
    assert_zero_stats(&stats);

    gc_debug_memory_layout layout = {(gc_debug_memory_block *)1, 1};
    TEST_CHECK(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_UNAVAILABLE);
    assert_zero_layout(&layout);
    gc_debug_memory_layout_dispose(&layout);
#endif

    return 0;
}
