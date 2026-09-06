#ifndef CWITHGC_GC_DEBUG_H
#define CWITHGC_GC_DEBUG_H

#include "gc.h"

#ifdef __cplusplus
extern "C" {
#define CWITHGC_DEBUG_NOEXCEPT noexcept
#else
#define CWITHGC_DEBUG_NOEXCEPT
#endif

/*
 * Optional collector-inspection API.
 *
 * These declarations and symbols are always available. A library built with
 * inspection disabled reports GC_DEBUG_UNAVAILABLE instead of requiring
 * callers to coordinate a preprocessor definition with the library build.
 * Unless documented otherwise, the runtime lifecycle and thread-safety rules
 * in gc.h also apply here.
 */

typedef enum {
    GC_DEBUG_OK,
    GC_DEBUG_UNAVAILABLE,
    GC_DEBUG_NOT_INITIALIZED,
    GC_DEBUG_INVALID_ARGUMENT,
    GC_DEBUG_OUT_OF_MEMORY,
    GC_DEBUG_INTERNAL_ERROR
} gc_debug_status;

typedef enum { GC_DEBUG_BLOCK_ALLOCATED, GC_DEBUG_BLOCK_FREE } gc_debug_block_state;

typedef struct {
    const void *start;
    size_t size;
    gc_debug_block_state state;
} gc_debug_memory_block;

typedef struct {
    gc_debug_memory_block *blocks;
    size_t block_count;
} gc_debug_memory_layout;

typedef struct {
    size_t heap_capacity;
    size_t free_bytes;
    size_t reclaimed_block_count; /* Blocks found unreachable and reclaimed since gc_init(). */
    size_t relocated_block_count; /* Blocks relocated since gc_init(). */
    size_t metadata_size;
    size_t root_count;
} gc_debug_stats;

/**
 * @brief Report whether this library was built with collector inspection.
 *
 * This query is independent of the collector lifecycle.
 *
 * @return 1 when inspection is available, otherwise 0.
 */
int gc_debug_is_available(void) CWITHGC_DEBUG_NOEXCEPT;

/**
 * @brief Read a consistent snapshot of collector statistics.
 *
 * A null output pointer returns GC_DEBUG_INVALID_ARGUMENT. Otherwise, *out is
 * cleared before any availability, lifecycle, or internal-error status is
 * returned. An initialized runtime is required when inspection is available.
 */
gc_debug_status gc_debug_get_stats(gc_debug_stats *out) CWITHGC_DEBUG_NOEXCEPT;

/**
 * @brief Snapshot the collector's current memory regions.
 *
 * A null output pointer returns GC_DEBUG_INVALID_ARGUMENT. Otherwise, *out is
 * cleared before any error status is returned. On success, blocks contains
 * block_count entries and must later be released with
 * gc_debug_memory_layout_dispose(). Ordering is collector-specific.
 *
 * Mark-and-sweep and copying collectors report their physical allocated and
 * free regions. The reference-counting collector reports only its live,
 * noncontiguous allocations; its remaining allocation budget is available as
 * gc_debug_stats.free_bytes rather than as a synthetic free block.
 */
gc_debug_status gc_debug_snapshot_memory_layout(gc_debug_memory_layout *out) CWITHGC_DEBUG_NOEXCEPT;

/**
 * @brief Release a memory-layout snapshot and clear it.
 *
 * Passing null is allowed. A successfully disposed layout has a null blocks
 * pointer and a zero block_count, so repeated disposal is safe.
 */
void gc_debug_memory_layout_dispose(gc_debug_memory_layout *layout) CWITHGC_DEBUG_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef CWITHGC_DEBUG_NOEXCEPT

#endif // CWITHGC_GC_DEBUG_H
