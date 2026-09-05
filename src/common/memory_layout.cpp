#include "gc.h"

#ifdef GC_DEBUG
extern "C" void gc_mem_layout_free(mem_block_info *layout) noexcept {
    delete[] layout;
}
#endif
