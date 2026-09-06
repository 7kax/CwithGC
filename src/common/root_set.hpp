#ifndef CWITHGC_COMMON_ROOT_SET_HPP
#define CWITHGC_COMMON_ROOT_SET_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/runtime.hpp"

namespace gc_runtime {

struct root_entry {
    void **pointer;
};

struct scope_boundary {
    std::uint64_t token;
    std::size_t start;
};

class root_set {
  public:
    using scope_token = std::uint64_t;

    scope_token begin_scope() {
        const scope_token token = next_token();
        scopes_.push_back({token, entries_.size()});
        return token;
    }

    void add(void *pointer_address) {
        if (scopes_.empty())
            gc_runtime::fatal("gc_local_var requires an active GC scope");

        auto **pointer = static_cast<void **>(pointer_address);
        entries_.push_back({pointer});
        *pointer = nullptr;
    }

    template <typename Visitor> void for_each(Visitor &&visitor) const noexcept {
        for (const root_entry &entry : entries_)
            visitor(entry.pointer);
    }

    void end_scope(scope_token token) noexcept {
        const std::size_t start = scope_start(token);
        entries_.resize(start);
        scopes_.pop_back();
    }

    template <typename Releaser> void end_scope(scope_token token, Releaser &&releaser) noexcept {
        const std::size_t start = scope_start(token);
        for (std::size_t i = entries_.size(); i > start; --i) {
            void **pointer = entries_[i - 1].pointer;
            if (*pointer != nullptr)
                releaser(*pointer);
        }
        entries_.resize(start);
        scopes_.pop_back();
    }

    void clear() noexcept {
        std::vector<root_entry>().swap(entries_);
        std::vector<scope_boundary>().swap(scopes_);
    }

    std::size_t size() const noexcept { return entries_.size(); }

  private:
    scope_token next_token() noexcept {
        if (next_token_ == 0)
            gc_runtime::fatal("GC scope token exhausted");

        return next_token_++;
    }

    std::size_t scope_start(scope_token token) const noexcept {
        if (scopes_.empty())
            gc_runtime::fatal("GC scope token is not active");
        if (scopes_.back().token != token)
            gc_runtime::fatal("GC scope end must follow LIFO order");

        return scopes_.back().start;
    }

    std::vector<root_entry> entries_;
    std::vector<scope_boundary> scopes_;
    scope_token next_token_ = 1;
};

} // namespace gc_runtime

#endif // CWITHGC_COMMON_ROOT_SET_HPP
