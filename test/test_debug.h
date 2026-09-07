#ifndef TEST_DEBUG_H
#define TEST_DEBUG_H

#include "gc_debug.h"

#include "test_check.h"

#include <stddef.h>

static inline gc_debug_stats test_gc_stats(void) {
    gc_debug_stats stats = {0};
    TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_OK);
    return stats;
}

static inline void test_gc_assert_not_initialized(void) {
    gc_debug_stats stats = {1, 2, 3, 4, 5, 6};
    TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_NOT_INITIALIZED);
    TEST_CHECK(stats.heap_capacity == 0);
    TEST_CHECK(stats.free_bytes == 0);
    TEST_CHECK(stats.reclaimed_block_count == 0);
    TEST_CHECK(stats.relocated_block_count == 0);
    TEST_CHECK(stats.metadata_size == 0);
    TEST_CHECK(stats.root_count == 0);
}

static inline size_t test_gc_heap_capacity(void) {
    return test_gc_stats().heap_capacity;
}

static inline size_t test_gc_free_bytes(void) {
    return test_gc_stats().free_bytes;
}

static inline size_t test_gc_reclaimed_block_count(void) {
    return test_gc_stats().reclaimed_block_count;
}

static inline size_t test_gc_relocated_block_count(void) {
    return test_gc_stats().relocated_block_count;
}

static inline size_t test_gc_metadata_size(void) {
    return test_gc_stats().metadata_size;
}

static inline size_t test_gc_root_count(void) {
    return test_gc_stats().root_count;
}

static inline gc_debug_memory_layout test_gc_memory_layout(void) {
    gc_debug_memory_layout layout = {NULL, 0};
    TEST_CHECK(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_OK);
    return layout;
}

static inline void test_gc_dispose_memory_layout(gc_debug_memory_layout *layout) {
    gc_debug_memory_layout_dispose(layout);
    if (layout != NULL) {
        TEST_CHECK(layout->blocks == NULL);
        TEST_CHECK(layout->block_count == 0);
    }
}

#endif // TEST_DEBUG_H
