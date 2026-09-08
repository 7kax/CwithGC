#ifndef CWITHGC_COMMON_ROOT_SET_HPP
#define CWITHGC_COMMON_ROOT_SET_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/runtime.hpp"

namespace gc_runtime {

struct RootEntry {
    void *slot;
};

struct ScopeBoundary {
    std::uint64_t token;
    std::size_t start;
};

class RootSet {
  public:
    using ScopeToken = std::uint64_t;

    ScopeToken begin_scope() {
        const ScopeToken token = next_token();
        scopes_.push_back({token, entries_.size()});
        return token;
    }

    void add(void *root_slot) {
        if (scopes_.empty())
            gc_runtime::fatal("gc_scope_add_root requires an active GC scope");

        entries_.push_back({root_slot});
        store_pointer(root_slot, nullptr);
    }

    template <typename Visitor> void for_each(Visitor &&visitor) const noexcept {
        for (const RootEntry &entry : entries_)
            visitor(entry.slot);
    }

    void end_scope(ScopeToken token) noexcept {
        const std::size_t start = scope_start(token);
        entries_.resize(start);
        scopes_.pop_back();
    }

    template <typename Releaser> void end_scope(ScopeToken token, Releaser &&releaser) noexcept {
        const std::size_t start = scope_start(token);
        for (std::size_t i = entries_.size(); i > start; --i) {
            void *slot = entries_[i - 1].slot;
            void *pointer = load_pointer(slot);
            if (pointer != nullptr)
                releaser(pointer);
        }
        entries_.resize(start);
        scopes_.pop_back();
    }

    void clear() noexcept {
        std::vector<RootEntry>().swap(entries_);
        std::vector<ScopeBoundary>().swap(scopes_);
    }

    std::size_t size() const noexcept { return entries_.size(); }

  private:
    ScopeToken next_token() noexcept {
        if (next_token_ == 0)
            gc_runtime::fatal("GC scope token exhausted");

        return next_token_++;
    }

    std::size_t scope_start(ScopeToken token) const noexcept {
        if (scopes_.empty())
            gc_runtime::fatal("GC scope token is not active");
        if (scopes_.back().token != token)
            gc_runtime::fatal("GC scope end must follow LIFO order");

        return scopes_.back().start;
    }

    std::vector<RootEntry> entries_;
    std::vector<ScopeBoundary> scopes_;
    ScopeToken next_token_ = 1;
};

} // namespace gc_runtime

#endif // CWITHGC_COMMON_ROOT_SET_HPP
