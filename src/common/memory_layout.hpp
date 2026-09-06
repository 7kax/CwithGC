#ifndef CWITHGC_COMMON_MEMORY_LAYOUT_HPP
#define CWITHGC_COMMON_MEMORY_LAYOUT_HPP

#include "gc_debug.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace gc_layout {

class LayoutBuilder {
  public:
    void add(const void *start, std::size_t size, gc_debug_block_state state) {
        blocks_.push_back({start, size, state});
    }

    gc_debug_memory_layout build() const {
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

#endif // CWITHGC_COMMON_MEMORY_LAYOUT_HPP
