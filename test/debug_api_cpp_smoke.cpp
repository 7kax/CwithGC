#include "gc_debug.h"
#include "test_check.h"

static_assert(noexcept(gc_debug_is_available()));
static_assert(noexcept(gc_debug_get_stats(nullptr)));
static_assert(noexcept(gc_debug_snapshot_memory_layout(nullptr)));
static_assert(noexcept(gc_debug_memory_layout_dispose(nullptr)));

int main() {
    TEST_CHECK(gc_debug_get_stats(nullptr) == GC_DEBUG_INVALID_ARGUMENT);
    TEST_CHECK(gc_debug_snapshot_memory_layout(nullptr) == GC_DEBUG_INVALID_ARGUMENT);

    gc_debug_stats stats{};
    gc_debug_memory_layout layout{};
    if (gc_debug_is_available() != 0) {
        TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_NOT_INITIALIZED);
        TEST_CHECK(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_NOT_INITIALIZED);
    } else {
        TEST_CHECK(gc_debug_get_stats(&stats) == GC_DEBUG_UNAVAILABLE);
        TEST_CHECK(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_UNAVAILABLE);
    }
    gc_debug_memory_layout_dispose(&layout);
    return 0;
}
