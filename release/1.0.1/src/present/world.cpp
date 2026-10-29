#include <zsdk/present.h>

#include "../detail/engine_core.h"
#include "../detail/runtime.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>

namespace zsdk::world
{
    namespace
    {
        constexpr uint64_t kWatchMs = 500;
        constexpr uint64_t kInstanceRetryMs = 1000;
        constexpr uint64_t kSettleMs = 3000;
        constexpr uint64_t kThrottleMs = 60000;
        constexpr int32_t kPovShift = 0x10;
        constexpr double kBound = 5.0e6;
        constexpr uint32_t kLoading = 0x3400u;

        Grip w;
        std::mutex shownGate;
        Grip shown;
        std::unordered_map<const void *, int32_t> povs;
        std::unordered_map<std::string, uint64_t> throttled;
        std::atomic<bool> pausedNow{ false };
        std::atomic<uint32_t> epochSeen{ 0 };

        const WorldVerbs &Verbs()
        {
            static const WorldVerbs checked = [] {
                namespace common = verbs::common;
                const App &a = app::Get();
                WorldVerbs v = a.world;
                int *roles[common::kWorldCount] = { &v.controller, &v.pawn, &v.camera, &v.seconds, &v.paused, &v.frames };
                for (int role = 0; role < common::kWorldCount; ++role)
                {
                    int &verb = *roles[role];
                    if (verb < 0 || common::Plays(a.specs, verb, static_cast<common::World>(role))) continue;
                    const verbs::Spec &want = common::kWorld[role];
                    const bool inside = static_cast<size_t>(verb) < a.specs.size() && a.specs[static_cast<size_t>(verb)].name;
                    Log("world: WorldVerbs names verb %d (%s) for %s.%s, the role is off; set it to the index of %s.%s in App::specs, or -1", verb,
                        inside ? verbs::KeyOf(a.specs[static_cast<size_t>(verb)]).c_str() : "outside the table", want.owner, want.name, want.owner, want.name);
                    verb = -1;
                }
                return v;
            }();
            return checked;
        }

        bool Quiet(const std::string &tag, uint64_t now, uint64_t gap = kThrottleMs)
        {
            uint64_t &at = throttled[tag];
            if (at && Age(now, at) < gap) return true;
            at = now;
            if (throttled.size() > 2048) throttled.clear();
            return false;
        }

        struct Pov
        {
            double loc[3];
            double rot[3];
            float fov;
        };

        void Shift(const char *why, const void *world, bool sameWorld, uint64_t now)
        {
            ++w.epoch;
            w.since = now;
            w.seconds = -1;
            w.stable = false;
            const Hooks &hooks = zsdk::hooks::Get();
            if (hooks.turn) hooks.turn(zsdk::world::Turn{ why, world, sameWorld, w.epoch });
            povs.clear();
            detail::engine::ClearHosts();
            detail::census::Turn(w.epoch);
            Log("world: %s epoch %u (%s)", world ? ue::Label(world).c_str() : "-", w.epoch, why);
        }
    }

    Grip &Hold()
    {
        if (!zsdk::thread::Hooked() || zsdk::thread::OnGame()) return w;
        Rare("world hold off thread", "world: the grip was read outside the game thread, this thread gets its own copy from world::Snapshot()");
        return detail::world::Spare();
    }

    Grip Snapshot()
    {
        std::unique_lock<std::mutex> hold(shownGate, std::defer_lock);
        if (!detail::Exiting()) hold.lock();
        else if (!hold.try_lock()) return Grip{};
        return shown;
    }

    bool Paused()
    {
        return pausedNow.load();
    }

    uint32_t Epoch()
    {
        return epochSeen.load();
    }

    const void *WorldOf(const void *thing)
    {
        const void *walk = ue::OuterOf(thing);
        for (int depth = 0; walk && depth < 8; ++depth)
        {
            const void *type = ue::ClassOf(walk);
            if (type && type == w.worldType) return walk;
            if (type && ue::Label(type) == "World")
            {
                w.worldType = type;
                return walk;
            }
            walk = ue::OuterOf(walk);
        }
        return nullptr;
    }

    const void *OwningWorld(const void *thing)
    {
        const void *walk = thing;
        for (int depth = 0; walk && depth < 8; ++depth)
        {
            if (ue::Doomed(walk) || (ue::Flags(walk) & kLoading) != 0) return nullptr;
            const void *type = ue::ClassOf(walk);
            if (ue::Inherits(type, "Level"))
            {
                const int32_t at = ue::PropertyOffset(type, "OwningWorld");
                const void *world = at >= 0 ? ue::Grab(walk, at) : nullptr;
                if (!world || ue::Doomed(world) || (ue::Flags(world) & kLoading) != 0 || !ue::Inherits(ue::ClassOf(world), "World")) return nullptr;
                return world;
            }
            walk = ue::OuterOf(walk);
        }
        return nullptr;
    }

    bool Look(const void *lens, const void *type, int32_t at, math::Vec3 &loc, math::Rot3 &rot)
    {
        if (!lens || !type || at < 0 || !ue::Here(lens, type)) return false;
        Pov pov{};
        if (!ue::Copy(static_cast<const uint8_t *>(lens) + at, &pov, sizeof(pov))) return false;
        for (int i = 0; i < 3; ++i)
        {
            if (!std::isfinite(pov.loc[i]) || std::fabs(pov.loc[i]) > kBound) return false;
            if (!std::isfinite(pov.rot[i]) || std::fabs(pov.rot[i]) > 36000.0) return false;
        }
        if (!std::isfinite(pov.fov) || pov.fov <= 5.0f || pov.fov >= 170.0f) return false;
        if (pov.loc[0] == 0.0 && pov.loc[1] == 0.0 && pov.loc[2] == 0.0) return false;
        loc = math::Vec3{ pov.loc[0], pov.loc[1], pov.loc[2] };
        rot = math::Rot3{ pov.rot[0], pov.rot[1], pov.rot[2] };
        return true;
    }

    int32_t PovAt(const void *lens, const void *type)
    {
        const auto found = povs.find(type);
        if (found != povs.end()) return found->second;
        const int32_t cache = ue::PropertyOffset(type, "CameraCachePrivate");
        if (cache < 0) return -1;
        math::Vec3 loc;
        math::Rot3 rot;
        if (!Look(lens, type, cache + kPovShift, loc, rot)) return -1;
        povs.emplace(type, cache + kPovShift);
        Log("camera %s pov +0x%X", ue::Label(type).c_str(), cache + kPovShift);
        return cache + kPovShift;
    }

    const void *Player(int verb, const void *context)
    {
        Call c(app::Get().specs, verb);
        c.Set("WorldContextObject", context);
        c.Set("PlayerIndex", int32_t{ 0 });
        Fire(c);
        const void *found = c.Get<const void *>("ReturnValue");
        return ue::Here(found) ? found : nullptr;
    }

    double Clock(const void *context)
    {
        Call c(app::Get().specs, Verbs().seconds);
        c.Set("WorldContextObject", context);
        Fire(c);
        const ue::Param &p = c.Parameter("ReturnValue");
        if (p.size == 8) return c.Get<double>("ReturnValue");
        if (p.size == 4) return static_cast<double>(c.Get<float>("ReturnValue"));
        throw std::runtime_error("unsupported world clock");
    }

    bool OpenWorld()
    {
        return text::Same(w.worldName, Verbs().map);
    }

    bool Paused(const void *context)
    {
        if (!context || !verbs::Ready(Verbs().paused)) return false;
        try
        {
            Call c(app::Get().specs, Verbs().paused);
            c.Set("WorldContextObject", context);
            Fire(c);
            return c.Yes("ReturnValue");
        }
        catch (const std::exception &error)
        {
            Rare("paused check", "check IsGamePaused failed: %s", error.what());
            return false;
        }
    }
}

namespace zsdk::detail::world
{
    using zsdk::world::Grip;

    void Seen()
    {
        zsdk::world::epochSeen = zsdk::world::w.epoch;
        std::lock_guard<std::mutex> hold(zsdk::world::shownGate);
        zsdk::world::shown = zsdk::world::w;
        zsdk::world::shown.at = GetTickCount64();
    }

    Grip &Spare()
    {
        thread_local Grip copy;
        copy = zsdk::world::Snapshot();
        return copy;
    }

    bool Advance()
    {
        Grip &w = zsdk::world::Hold();
        const int frames = zsdk::world::Verbs().frames;
        if (w.frameless || !verbs::Ready(frames)) return true;
        try
        {
            Call c(app::Get().specs, frames);
            Fire(c);
            const ue::Param &p = c.Parameter("ReturnValue");
            const int64_t frame = p.size == 8 ? c.Get<int64_t>("ReturnValue") : p.size == 4 ? static_cast<int64_t>(c.Get<int32_t>("ReturnValue")) : -1;
            if (frame < 0) return true;
            if (frame == w.frame) return false;
            w.frame = frame;
            return true;
        }
        catch (const Refused &error)
        {
            if (zsdk::detail::Halted() || zsdk::thread::Stopping()) return false;
            w.frameless = true;
            Log("frame counter unavailable, ticks are not gated by frame: %s", error.what());
            return true;
        }
        catch (const std::exception &error)
        {
            w.frameless = true;
            Log("frame counter unavailable, ticks are not gated by frame: %s", error.what());
            return true;
        }
    }

    void Watch(uint64_t now)
    {
        using namespace zsdk::world;
        const WorldVerbs &v = Verbs();
        if (w.polled && Age(now, w.polled) < kWatchMs) return;
        w.polled = now;
        const auto snap = zsdk::census::Current();
        if (w.gi && (!ue::Here(w.gi, w.giType) || ue::Doomed(w.gi)))
        {
            w.gi = nullptr;
            w.giType = nullptr;
        }
        if (!w.gi && snap && snap->gi && ue::Here(snap->gi))
        {
            w.gi = snap->gi;
            w.giType = ue::ClassOf(snap->gi);
            Log("game instance %s", ue::Path(w.gi).c_str());
        }
        const void *pilot = nullptr;
        const void *body = nullptr;
        const void *camera = nullptr;
        const void *world = nullptr;
        if (verbs::Ready(v.controller) && verbs::Ready(v.pawn) && verbs::Ready(v.camera))
        {
            try
            {
                if (w.gi) pilot = Player(v.controller, w.gi);
                if (!pilot && snap && (!w.probed || Age(now, w.probed) >= kInstanceRetryMs))
                {
                    w.probed = now;
                    for (const void *candidate : snap->gis)
                    {
                        if (!candidate || candidate == w.gi || !ue::Here(candidate) || ue::Gone(candidate)) continue;
                        const void *found = Player(v.controller, candidate);
                        if (!found) continue;
                        w.gi = candidate;
                        w.giType = ue::ClassOf(candidate);
                        pilot = found;
                        Log("game instance %s answers with player controller %s", ue::Path(candidate).c_str(), ue::Label(found).c_str());
                        break;
                    }
                }
                if (pilot)
                {
                    body = Player(v.pawn, w.gi);
                    camera = Player(v.camera, w.gi);
                    world = WorldOf(body ? body : pilot);
                }
            }
            catch (const std::exception &error)
            {
                pilot = body = camera = world = nullptr;
                if (!Quiet("player lookup", now)) Log("player lookup: %s", error.what());
            }
        }
        double seconds = -1;
        if (body && verbs::Ready(v.seconds))
        {
            try
            {
                seconds = Clock(body);
            }
            catch (const std::exception &error)
            {
                seconds = -1;
                if (!Quiet("world clock", now)) Log("world clock: %s", error.what());
            }
            if (!std::isfinite(seconds) || seconds < 0) seconds = -1;
        }
        const bool rewound = seconds >= 0 && w.seconds >= 0 && seconds + 1.0 < w.seconds;
        if ((world && world != w.home) || rewound)
        {
            const bool sameWorld = rewound && world && world == w.home;
            Shift(sameWorld ? "clock rewound, same world" : rewound ? "clock rewound, new world" : w.first ? "first" : "world changed", world, sameWorld, now);
            w.first = false;
            if (world) w.home = world;
        }
        else if (body != w.body)
        {
            w.since = now;
            if (body) Log("world: pawn changed in the same world: %s", ue::Label(body).c_str());
        }
        const bool paused = body && world && Paused(body);
        if (paused != w.paused) Log("world: %s", paused ? "the game is paused, the world clock stops" : "the game runs again");
        w.paused = paused;
        pausedNow = paused;
        w.growing = seconds >= 0 && w.seconds >= 0 && seconds > w.seconds + 0.01;
        if (seconds >= 0) w.seconds = seconds;
        w.pilot = pilot;
        w.body = body;
        w.bodyType = body ? ue::ClassOf(body) : nullptr;
        w.lens = camera;
        w.lensType = camera ? ue::ClassOf(camera) : nullptr;
        w.world = world;
        w.worldName = world ? ue::Label(world) : std::string();
        w.povAt = camera ? PovAt(camera, w.lensType) : -1;
        const bool fit = pilot && body && world && OpenWorld();
        const bool stable = fit && Age(now, w.since) >= kSettleMs && (w.growing || (w.paused && w.stable));
        if (stable != w.stable)
        {
            if (stable) Log("world: stable after %.1f s, %s, time %.1f", static_cast<double>(Age(now, w.since)) / 1000.0, w.worldName.c_str(), w.seconds);
            else Log("world: unsettled, epoch %u, %s", w.epoch, w.worldName.empty() ? "-" : w.worldName.c_str());
        }
        w.wait = !fit ? "wait.world" : !stable ? "wait.settle" : !zsdk::census::Consoles() ? "wait.consoles" : "";
        w.fit = fit;
        w.stable = stable;
    }

#if defined(ZSDK_TESTS)
    void Jolt(const char *why, uint64_t now)
    {
        zsdk::world::Shift(why, nullptr, false, now);
    }

    void Latch(bool hold)
    {
        if (hold) zsdk::world::shownGate.lock();
        else zsdk::world::shownGate.unlock();
    }
#endif
}
