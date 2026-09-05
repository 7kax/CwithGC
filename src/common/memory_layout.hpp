#ifndef CWITHGC_COMMON_MEMORY_LAYOUT_HPP
#define CWITHGC_COMMON_MEMORY_LAYOUT_HPP

#include "gc.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace gc_layout {

class layout_builder {
  public:
    void add(void *start, std::size_t size, bool is_free) {
        blocks_.push_back({start, size, is_free ? 1 : 0});
    }

    mem_block_info *release() const {
        auto layout = std::make_unique<mem_block_info[]>(blocks_.size() + 1);
        std::copy(blocks_.begin(), blocks_.end(), layout.get());
        layout[blocks_.size()] = {nullptr, 0, 0};
        return layout.release();
    }

  private:
    std::vector<mem_block_info> blocks_;
};

} // namespace gc_layout

#endif // CWITHGC_COMMON_MEMORY_LAYOUT_HPP
