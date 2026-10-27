#include <zsdk/present.h>

#include "../detail/quit.h"

#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace zsdk::guard
{
    namespace
    {
        std::mutex gate;
        std::map<const Set *, State> states;
        std::map<std::string, bool> confirmed;
        constexpr const Set *kOwn[] = { &zsdk::detail::quit::kSet61bc1e03, &zsdk::detail::quit::kSet2bd3f115 };
        static_assert(std::size(kOwn) == std::size(zsdk::detail::quit::kExits));

        bool Cached(const Field &field, bool &value)
        {
            std::lock_guard<std::mutex> hold(gate);
            const auto found = confirmed.find(field.name ? field.name : "");
            if (found == confirmed.end()) return false;
            value = found->second;
            return true;
        }

        void Listed(const Set &set)
        {
            for (const Set *one : app::Get().sets)
                if (one == &set) return;
            for (const Set *one : kOwn)
                if (one == &set) return;
            const char *name = set.name ? set.name : "-";
            const std::string tag = std::string("guard unlisted ") + name;
            Rare(tag.c_str(), "guard: set %s is not in App::sets, so the SDK checks it only when it is used and the guards suite does not see it", name);
        }
    }

    bool Hex(const char *bytes, std::vector<int> &out)
    {
        out.clear();
        if (!bytes) return false;
        const std::string text(bytes);
        size_t i = 0;
        while (i < text.size())
        {
            if (text[i] == ' ')
            {
                ++i;
                continue;
            }
            if (i + 1 >= text.size()) return false;
            if (text[i] == '?' && text[i + 1] == '?') out.push_back(-1);
            else
            {
                char pair[3] = { text[i], text[i + 1], 0 };
                char *end = nullptr;
                const long value = std::strtol(pair, &end, 16);
                if (!end || *end) return false;
                out.push_back(static_cast<int>(value));
            }
            i += 2;
        }
        return !out.empty();
    }

    bool Matches(uintptr_t at, const char *bytes)
    {
        std::vector<int> want;
        if (!Hex(bytes, want)) return false;
        std::vector<uint8_t> got(want.size());
        if (!ue::Copy(reinterpret_cast<const void *>(at), got.data(), got.size())) return false;
        for (size_t i = 0; i < want.size(); ++i)
            if (want[i] >= 0 && got[i] != static_cast<uint8_t>(want[i])) return false;
        return true;
    }

    uintptr_t Base()
    {
        return exe::Base();
    }

    uintptr_t At(const Anchor &anchor)
    {
        return Base() + static_cast<uintptr_t>(anchor.va - kImageBase);
    }

    uintptr_t Target(uintptr_t at, int disp, int next)
    {
        int32_t offset = 0;
        if (!ue::Copy(reinterpret_cast<const void *>(at + static_cast<uintptr_t>(disp)), &offset, sizeof(offset))) return 0;
        return at + static_cast<uintptr_t>(next) + static_cast<uintptr_t>(static_cast<intptr_t>(offset));
    }

    uintptr_t Resolve(const Anchor &anchor)
    {
        const uintptr_t at = At(anchor);
        return Matches(at, anchor.bytes) ? Target(at, anchor.disp, anchor.next) : 0;
    }

    const Build *Find(std::string_view sha)
    {
        for (const Build *build : kBuilds)
            if (sha == build->sha256) return build;
        return nullptr;
    }

    std::span<const Set *const> Own()
    {
        return kOwn;
    }

    Report Check(const Set &set)
    {
        Report r;
        const uintptr_t base = Base();
        r.total = static_cast<int>(set.count);
        for (size_t i = 0; i < set.count; ++i)
        {
            const Guard &g = set.guards[i];
            if (Matches(base + g.rva, g.bytes)) ++r.ok;
            else r.bad += text::Format("%s0x%X", r.bad.empty() ? "" : " ", g.rva);
        }
        r.image = exe::ImageSize();
        r.imageOk = set.build && r.image == set.build->imageSize;
        r.known = set.build && exe::Sha() == set.build->sha256;
        bool fine = r.ok == r.total;
        if (set.need == Need::ImageSize || set.need == Need::KnownBuild) fine = fine && r.imageOk;
        if (set.need == Need::KnownBuild && !exe::Hashed()) r.state = State::Pending;
        else r.state = fine && (set.need != Need::KnownBuild || r.known) ? State::Ok : State::Off;
        std::lock_guard<std::mutex> hold(gate);
        states[&set] = r.state;
        return r;
    }

    State Of(const Set &set)
    {
        State now = State::Pending;
        bool seen = false;
        {
            std::lock_guard<std::mutex> hold(gate);
            const auto found = states.find(&set);
            seen = found != states.end();
            if (seen) now = found->second;
        }
        if (!seen) Listed(set);
        if (now == State::Pending && exe::Hashed()) now = Check(set).state;
        return now;
    }

    bool Ready(const Set &set)
    {
        return Of(set) != State::Pending;
    }

    int Recheck()
    {
        std::vector<const Set *> waiting;
        {
            std::lock_guard<std::mutex> hold(gate);
            for (const auto &[set, state] : states)
                if (state == State::Pending) waiting.push_back(set);
        }
        int done = 0;
        for (const Set *set : waiting)
        {
            const Report r = Check(*set);
            ++done;
            Log("guard set %s: %d/%d match after the exe hash, %s", set->name ? set->name : "-", r.ok, r.total, r.state == State::Ok ? "on" : r.state == State::Off ? "off" : "pending");
        }
        return done;
    }

    bool Usable(const Field &field)
    {
        switch (field.proof)
        {
        case Proof::Guard:
        case Proof::Rip: return field.set && Of(*field.set) == State::Ok;
        case Proof::Reflected:
        {
            bool value = false;
            return Cached(field, value) && value;
        }
        default: return false;
        }
    }

    bool Confirm(const Field &field)
    {
        if (field.proof != Proof::Reflected) return Usable(field);
        const void *type = ue::FindClass(field.reflectedClass);
        const int32_t at = type ? ue::PropertyOffset(type, field.reflectedProperty) : -1;
        const bool same = at >= 0 && static_cast<uint32_t>(at) == field.at;
        if (!same) Rare(field.name, "guard: field %s is +0x%X in %s.%s, expected +0x%X, its features are off", field.name, at, field.reflectedClass, field.reflectedProperty, field.at);
        if (ue::Bound())
        {
            std::lock_guard<std::mutex> hold(gate);
            confirmed[field.name ? field.name : ""] = same;
        }
        return same;
    }

    bool Use(const Field &field, uintptr_t &at)
    {
        at = 0;
        bool ok = Usable(field);
        bool known = true;
        if (!ok && field.proof == Proof::Reflected)
        {
            bool value = false;
            known = Cached(field, value);
            if (!known && ue::Bound()) ok = Confirm(field);
        }
        if (ok)
        {
            at = field.at;
            return true;
        }
        if (!known) return false;
        if ((field.proof == Proof::Guard || field.proof == Proof::Rip) && field.set && Of(*field.set) == State::Pending)
        {
            Note(2, "guard: field %s waits, guard verdict pending for set %s", field.name, field.set->name ? field.set->name : "-");
            return false;
        }
        const std::string tag = std::string("use ") + (field.name ? field.name : "-");
        Rare(tag.c_str(), "guard: field %s is not usable on this build (%s), its read or write is refused", field.name,
             field.proof == Proof::Unproven ? "unproven" : field.proof == Proof::Reflected ? "reflection differs" : "guard set is not on");
        return false;
    }
}
