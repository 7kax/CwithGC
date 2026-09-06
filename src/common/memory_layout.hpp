#ifndef CWITHGC_COMMON_MEMORY_LAYOUT_HPP
#define CWITHGC_COMMON_MEMORY_LAYOUT_HPP

#include "gc.h"
#include "gc_debug.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace gc_layout {

class layout_builder {
  public:
    void add(const void *start, std::size_t size, gc_debug_block_state state) {
        blocks_.push_back({start, size, state});
    }

    gc_debug_memory_layout release() const {
        if (blocks_.empty())
            return {nullptr, 0};

        auto blocks = std::make_unique<gc_debug_memory_block[]>(blocks_.size());
        std::copy(blocks_.begin(), blocks_.end(), blocks.get());
        return {blocks.release(), blocks_.size()};
    }

  private:
    std::vector<gc_debug_memory_block> blocks_;
};

} // namespace gc_layout

#ifdef GC_DEBUG
namespace gc_layout {

inline mem_block_info *release_legacy(gc_debug_memory_layout layout) {
    std::unique_ptr<gc_debug_memory_block[]> blocks(layout.blocks);
    auto legacy = std::make_unique<mem_block_info[]>(layout.block_count + 1);

    for (std::size_t i = 0; i < layout.block_count; ++i) {
        const gc_debug_memory_block &block = blocks[i];
        legacy[i] = {const_cast<void *>(block.start), block.size,
                     block.state == GC_DEBUG_BLOCK_FREE ? 1 : 0};
    }
    legacy[layout.block_count] = {nullptr, 0, 0};
    return legacy.release();
}

} // namespace gc_layout
#endif

#endif // CWITHGC_COMMON_MEMORY_LAYOUT_HPP
