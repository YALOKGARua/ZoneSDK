#include <zsdk/present.h>

#include "../detail/engine_core.h"
#include "../detail/ledger_raw.h"
#include "../detail/runtime.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace zsdk::census
{
    namespace
    {
        constexpr uint64_t kSweepMs = 2000;
        constexpr uint64_t kCalmSweepMs = 10000;
        constexpr uint64_t kWantedGapMs = 3000;
        constexpr const char *kHookKey = "census.hook";

        struct Kind
        {
            int lib = -1;
            int owner = -1;
            bool gi = false;
        };

        std::mutex gate;
        std::shared_ptr<const Snapshot> current;
        std::atomic<uint32_t> epoch{ 0 };
        std::atomic<bool> wanted{ false };
        std::atomic<int> sweeps{ 0 };
        std::atomic<bool> hookOff{ false };
        bool hookBlocked = false;
        std::unordered_map<const void *, Kind> kinds;

        Kind Classify(const void *type)
        {
            const CensusSpec &spec = app::Get().census;
            Kind k;
            std::vector<std::string> chain;
            const void *walk = type;
            for (int depth = 0; depth < 40 && walk; ++depth)
            {
                chain.push_back(ue::Label(walk));
                walk = ue::Grab(walk, ue::kSuperAt);
            }
            if (chain.empty()) return k;
            for (int i = 0; i < spec.staticCount && i < static_cast<int>(kSlots); ++i)
                if (chain.front() == spec.statics[i]) k.lib = i;
            for (const std::string &one : chain)
            {
                if (one == "GameInstance") k.gi = true;
                for (int i = 0; i < spec.hostCount && i < static_cast<int>(kSlots); ++i)
                    if (one == spec.hosts[i] && k.owner < 0) k.owner = i;
            }
            return k;
        }

        bool Sweep(Snapshot &out)
        {
            const double from = Ms();
            detail::engine::Survey();
            const int32_t count = ue::Total();
            int32_t index = 0;
            for (; index < count && !thread::Stopping(); ++index)
            {
                const void *thing = ue::At(index);
                if (!thing) continue;
                const void *type = ue::ClassOf(thing);
                if (!type) continue;
                auto found = kinds.find(type);
                if (found == kinds.end()) found = kinds.emplace(type, Classify(type)).first;
                const Kind &k = found->second;
                if (k.lib < 0 && !k.gi && k.owner < 0) continue;
                const std::string name = ue::Label(thing);
                if (name.empty()) continue;
                const bool cdo = name.rfind("Default__", 0) == 0;
                if (k.lib >= 0)
                {
                    if (cdo && !out.statics[static_cast<size_t>(k.lib)]) out.statics[static_cast<size_t>(k.lib)] = thing;
                    continue;
                }
                if (cdo || ue::Gone(thing)) continue;
                if (k.owner >= 0) out.hosts[static_cast<size_t>(k.owner)] = thing;
                if (!k.gi) continue;
                out.gi = thing;
                if (out.gis.size() < kInstanceCap) out.gis.push_back(thing);
            }
            out.sweepMs = Ms() - from;
            out.objects = count;
            return index >= count;
        }

        bool Allowed()
        {
            if (hookOff.load()) return false;
            if (ledger::Blocked(kHookKey))
            {
                if (!hookBlocked) Log("census: hooks.census and hooks.complete are disabled after a crash, Try again in the tab clears it");
                hookBlocked = true;
                return false;
            }
            hookBlocked = false;
            return true;
        }

        template<class F> bool Guarded(const char *hook, F &&work)
        {
            const detail::Fended fended = detail::Fend(work);
            if (fended.thrown) Rare(hook, "census: %s error: %s", hook, fended.what.c_str());
            if (!fended.code) return !fended.thrown;
            hookOff = true;
            Rare("census hook seh", "census: structured exception 0x%08X in %s, hooks.census and hooks.complete are off for this session and %s is disabled until Try again", fended.code, hook, kHookKey);
            ledger::raw::Disable(kHookKey);
            return false;
        }

        bool Complete(const Snapshot *last)
        {
            if (!last) return false;
            const Hooks &hooks = zsdk::hooks::Get();
            if (!hooks.complete || !Allowed()) return last->gi != nullptr;
            bool done = false;
            if (Guarded("hooks.complete", [&hooks, last, &done] { done = hooks.complete(*last); })) return done;
            return last->gi != nullptr;
        }
    }

    std::shared_ptr<const Snapshot> Current()
    {
        std::unique_lock<std::mutex> hold(gate, std::defer_lock);
        if (!detail::Exiting()) hold.lock();
        else if (!hold.try_lock()) return nullptr;
        return current;
    }

    void Want(const char *reason)
    {
        if (!wanted.exchange(true)) Note(1, "census: a sweep is wanted (%s)", reason ? reason : "-");
    }

    uint32_t Epoch()
    {
        return epoch.load();
    }

    int Sweeps()
    {
        return sweeps.load();
    }

    bool Fresh(const Snapshot &snapshot, uint32_t worldEpoch, uint64_t since)
    {
        return snapshot.epoch == worldEpoch && snapshot.at >= since;
    }

    const void *Host(size_t slot)
    {
        if (slot >= kSlots || static_cast<int>(slot) >= app::Get().census.hostCount) return nullptr;
        const auto snap = Current();
        const world::Grip &w = world::Hold();
        if (!snap || !Fresh(*snap, w.epoch, w.since)) return nullptr;
        const void *host = snap->hosts[slot];
        return host && !ue::Doomed(host) ? host : nullptr;
    }

    bool Consoles()
    {
        const int count = app::Get().census.hostCount;
        for (int i = 0; i < count && i < static_cast<int>(kSlots); ++i)
            if (!Host(static_cast<size_t>(i))) return false;
        return true;
    }
}

namespace zsdk::detail::census
{
    void Turn(uint32_t value)
    {
        zsdk::census::epoch = value;
    }

#if defined(ZSDK_TESTS)
    void Latch(bool hold)
    {
        if (hold) zsdk::census::gate.lock();
        else zsdk::census::gate.unlock();
    }
#endif

    void Tally(uint64_t &sweptAt, uint32_t &sweptEpoch)
    {
        using namespace zsdk::census;
        const uint32_t now32 = epoch.load();
        const uint64_t now = GetTickCount64();
        const auto last = Current();
        const bool complete = Complete(last.get());
        if (app::Get().policy.census == CensusMode::OnDemand)
        {
            const bool asked = wanted.load();
            if (now32 == sweptEpoch && sweptAt && complete && !asked) return;
            if (sweptAt && Age(now, sweptAt) < kWantedGapMs) return;
            wanted = false;
        }
        else
        {
            const uint64_t gap = complete ? kCalmSweepMs : kSweepMs;
            if (now32 == sweptEpoch && sweptAt && Age(now, sweptAt) < gap) return;
        }
        if (now32 != sweptEpoch) kinds.clear();
        auto fresh = std::make_shared<Snapshot>();
        fresh->epoch = now32;
        if (!Sweep(*fresh)) return;
        fresh->at = GetTickCount64();
        sweptAt = now;
        ++sweeps;
        if (now32 != sweptEpoch || !complete)
        {
            const Hooks &hooks = zsdk::hooks::Get();
            if (hooks.census && Allowed())
            {
                if (!zsdk::thread::Stopping()) Guarded("hooks.census", [&hooks, &fresh] { hooks.census(*fresh); });
            }
            else
                Note(1, "census: %d objects, game instance %s, statics %d/%d, %.1f ms", fresh->objects, fresh->gi ? "found" : "missing",
                     static_cast<int>(std::count_if(fresh->statics.begin(), fresh->statics.end(), [](const void *p) { return p != nullptr; })), app::Get().census.staticCount,
                     fresh->sweepMs);
        }
        sweptEpoch = now32;
        std::lock_guard<std::mutex> hold(gate);
        current = std::move(fresh);
    }
}
