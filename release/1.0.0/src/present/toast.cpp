#include <zsdk/present.h>

#include "../detail/runtime.h"

#include <windows.h>

#include <atomic>
#include <deque>
#include <mutex>

namespace zsdk::toast
{
    namespace
    {
        std::mutex gate;
        std::deque<Item> toasts;
        std::atomic<bool> focusedNow{ true };
    }

    void Push(int kind, const std::string &detail, const std::vector<std::string> &args, bool replaceKind)
    {
        std::lock_guard<std::mutex> hold(gate);
        Item t;
        t.kind = kind;
        t.detail = detail;
        t.args = args;
        t.born = GetTickCount64();
        if (replaceKind)
            while (!toasts.empty() && toasts.back().kind == kind) toasts.pop_back();
        toasts.push_back(std::move(t));
        while (toasts.size() > kKept) toasts.pop_front();
    }

    std::vector<Item> Live(uint64_t now)
    {
        std::lock_guard<std::mutex> hold(gate);
        while (!toasts.empty() && Age(now, toasts.front().born) > kLifeMs) toasts.pop_front();
        const size_t from = toasts.size() > kShown ? toasts.size() - kShown : 0;
        return std::vector<Item>(toasts.begin() + static_cast<std::ptrdiff_t>(from), toasts.end());
    }

    void Reset()
    {
        std::lock_guard<std::mutex> hold(gate);
        toasts.clear();
    }

    bool Focused()
    {
        return focusedNow.load();
    }
}

namespace zsdk::detail::toast
{
    void Focus(bool focused)
    {
        zsdk::toast::focusedNow = focused;
    }
}

namespace zsdk::hooks
{
    namespace
    {
        Hooks bound;
    }

    void Bind(const Hooks &hooks)
    {
        bound = hooks;
    }

    const Hooks &Get()
    {
        return bound;
    }
}
