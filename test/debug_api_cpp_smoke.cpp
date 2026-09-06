#include "gc_debug.h"

#include <cassert>

static_assert(noexcept(gc_debug_is_available()));
static_assert(noexcept(gc_debug_get_stats(nullptr)));
static_assert(noexcept(gc_debug_snapshot_memory_layout(nullptr)));
static_assert(noexcept(gc_debug_memory_layout_dispose(nullptr)));

int main() {
    assert(gc_debug_get_stats(nullptr) == GC_DEBUG_INVALID_ARGUMENT);
    assert(gc_debug_snapshot_memory_layout(nullptr) == GC_DEBUG_INVALID_ARGUMENT);

    gc_debug_stats stats{};
    gc_debug_memory_layout layout{};
    if (gc_debug_is_available() != 0) {
        assert(gc_debug_get_stats(&stats) == GC_DEBUG_NOT_INITIALIZED);
        assert(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_NOT_INITIALIZED);
    } else {
        assert(gc_debug_get_stats(&stats) == GC_DEBUG_UNAVAILABLE);
        assert(gc_debug_snapshot_memory_layout(&layout) == GC_DEBUG_UNAVAILABLE);
    }
    gc_debug_memory_layout_dispose(&layout);
    return 0;
}
