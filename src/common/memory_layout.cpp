#include "gc.h"
#include "gc_debug.h"

extern "C" void gc_debug_memory_layout_dispose(gc_debug_memory_layout *layout) noexcept {
    if (layout == nullptr)
        return;

    delete[] layout->blocks;
    layout->blocks = nullptr;
    layout->block_count = 0;
}

#ifdef GC_DEBUG
extern "C" void gc_mem_layout_free(mem_block_info *layout) noexcept {
    delete[] layout;
}
#endif
