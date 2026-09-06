#ifndef GC_DEBUG_H
#define GC_DEBUG_H

#include "gc.h"

#ifdef __cplusplus
extern "C" {
#define GC_DEBUG_NOEXCEPT noexcept
#else
#define GC_DEBUG_NOEXCEPT
#endif

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
    size_t reclaimed_blocks;
    size_t metadata_size;
    size_t root_count;
} gc_debug_stats;

int gc_debug_is_available(void) GC_DEBUG_NOEXCEPT;
gc_debug_status gc_debug_get_stats(gc_debug_stats *out) GC_DEBUG_NOEXCEPT;
gc_debug_status gc_debug_snapshot_memory_layout(gc_debug_memory_layout *out) GC_DEBUG_NOEXCEPT;
void gc_debug_memory_layout_dispose(gc_debug_memory_layout *layout) GC_DEBUG_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef GC_DEBUG_NOEXCEPT

#endif // GC_DEBUG_H
