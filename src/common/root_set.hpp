#ifndef CWITHGC_COMMON_ROOT_SET_HPP
#define CWITHGC_COMMON_ROOT_SET_HPP

#include <cstddef>
#include <vector>

namespace gc_runtime {

struct root_entry {
    void **pointer;
    void *frame;
};

class root_set {
  public:
    void add(void *pointer_address, void *frame_address) {
        auto **pointer = static_cast<void **>(pointer_address);
        entries_.push_back({pointer, frame_address});
        *pointer = nullptr;
    }

    template <typename Visitor> void for_each(Visitor &&visitor) const noexcept {
        for (const root_entry &entry : entries_)
            visitor(entry.pointer);
    }

    void pop_frame() noexcept {
        if (entries_.empty())
            return;

        void *frame_address = entries_.back().frame;
        while (!entries_.empty() && entries_.back().frame == frame_address)
            entries_.pop_back();
    }

    template <typename Releaser> void pop_frame(Releaser &&releaser) noexcept {
        if (entries_.empty())
            return;

        void *frame_address = entries_.back().frame;
        while (!entries_.empty() && entries_.back().frame == frame_address) {
            void **pointer = entries_.back().pointer;
            if (*pointer != nullptr)
                releaser(*pointer);
            entries_.pop_back();
        }
    }

    void clear() noexcept {
        std::vector<root_entry> empty;
        entries_.swap(empty);
    }

    std::size_t size() const noexcept { return entries_.size(); }

  private:
    std::vector<root_entry> entries_;
};

} // namespace gc_runtime

#endif // CWITHGC_COMMON_ROOT_SET_HPP
