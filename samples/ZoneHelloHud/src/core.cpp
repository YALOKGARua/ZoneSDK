#include <zsdk/present.h>

#include "hellohud.h"

#include <cstdio>
#include <mutex>
#include <string>

namespace zhellohud::core
{
    namespace
    {
        using zsdk::Log;
        using zsdk::Note;

        Settings cfg;
        uint64_t revision = 0;
        uint64_t triedAt = 0;
        Reading reading;
        int reads = 0;
        std::string refusal;
        std::mutex gate;
        Status shown;

        bool Live()
        {
            return cfg.enabled && cfg.shown;
        }

        void Tell(Say say, const std::string &arg)
        {
            if (cfg.toasts) Toast(say, arg);
        }

        void Miss(const char *why)
        {
            if (refusal != why) Log("hellohud: no reading, %s", why);
            refusal = why;
        }

        std::string KeyOf(int verb)
        {
            return zsdk::verbs::KeyOf(kSpecs[static_cast<size_t>(verb)]);
        }

        template<class T> bool Fetch(int verb, const void *body, T &out)
        {
            if (!zsdk::verbs::Ready(verb) || zsdk::ledger::Blocked(KeyOf(verb))) return false;
            zsdk::Call c(kSpecs, verb, body);
            zsdk::Fire(c);
            out = c.Get<T>("ReturnValue");
            return true;
        }

        void Read(uint64_t now)
        {
            const zsdk::world::Grip &w = zsdk::world::Hold();
            if (!w.stable || !zsdk::ue::Here(w.body, w.bodyType)) return Miss("why.world");
            if (!zsdk::verbs::Ready(kLocation)) return Miss("why.verbs");
            if (zsdk::ledger::Blocked(KeyOf(kLocation))) return Miss("why.crash");
            try
            {
                Reading r;
                Fetch(kLocation, w.body, r.at);
                r.moving = Fetch(kVelocity, w.body, r.velocity);
                r.turned = Fetch(kRotation, w.body, r.rot);
                r.when = now;
                reading = r;
                ++reads;
                if (!refusal.empty()) Log("hellohud: reading again after %s", refusal.c_str());
                refusal.clear();
            }
            catch (const zsdk::Fault &error)
            {
                Log("hellohud: %s", error.what());
                Miss("why.failed");
            }
            catch (const std::exception &error)
            {
                zsdk::Rare("hellohud read", "hellohud: reading failed: %s", error.what());
                Miss("why.failed");
            }
        }

        void Toggle()
        {
            Settings s = zsdk::ini::Config<Settings>();
            s.shown = !s.shown;
            zsdk::ini::Apply(s);
            cfg = s;
            revision = zsdk::ini::Revision();
            if (!s.shown) reading = {};
            Log("hellohud: the panel is %s", s.shown ? "shown" : "hidden");
            Tell(Say::Panel, s.shown ? "on" : "off");
        }
    }

    void Refresh(bool again)
    {
        const uint64_t now = zsdk::ini::Revision();
        if (now == revision && !again) return;
        revision = now;
        cfg = zsdk::ini::Config<Settings>();
        if (!Live()) reading = {};
    }

    void Prime()
    {
        const zsdk::verbs::Report &r = zsdk::verbs::Last();
        if (!r.coreReady) Log("hellohud: a required game function is missing, the panel stays empty");
        for (const int verb : { kVelocity, kRotation })
            if (!zsdk::verbs::Ready(verb)) Log("hellohud: %s is missing, the panel leaves that line out", KeyOf(verb).c_str());
        for (const std::string &key : zsdk::ledger::Booted()) Tell(Say::Crashed, key);
    }

    void Step(uint64_t now)
    {
        if (!Live() || zsdk::Age(now, triedAt) < view::kReadMs) return;
        triedAt = now;
        Read(now);
    }

    bool Hot()
    {
        return Live() && zsdk::world::Hold().stable;
    }

    void Publish(uint64_t)
    {
        const zsdk::world::Grip &w = zsdk::world::Hold();
        std::lock_guard<std::mutex> hold(gate);
        shown.coreReady = zsdk::verbs::Last().coreReady;
        shown.stable = w.stable;
        shown.live = Live();
        shown.wait = w.wait ? w.wait : "";
        shown.refusal = refusal;
        shown.reading = reading;
        shown.reads = reads;
    }

    void Turn(const zsdk::world::Turn &turn)
    {
        reading = {};
        Note(1, "hellohud: world turn %u (%s)", turn.epoch, turn.why);
    }

    void Order(int order)
    {
        if (order == static_cast<int>(Order::Toggle)) Toggle();
    }

    const char *OrderName(int order)
    {
        return order == static_cast<int>(Order::Toggle) ? "toggle" : "unknown";
    }

    bool Complete(const zsdk::census::Snapshot &snapshot)
    {
        return snapshot.gi && snapshot.statics[0] && snapshot.statics[1];
    }

    Status Snapshot()
    {
        std::lock_guard<std::mutex> hold(gate);
        Status s = shown;
        s.phase = zsdk::thread::State(s.stable);
        return s;
    }

    std::string Section()
    {
        const Status s = Snapshot();
        char line[256]{};
        std::snprintf(line, sizeof(line), "hellohud: panel %s, %d readings, last at %.0f %.0f %.0f, refusal %s", s.live ? "on" : "off", s.reads, s.reading.at.x, s.reading.at.y, s.reading.at.z,
                      s.refusal.empty() ? "-" : s.refusal.c_str());
        return line;
    }

    void Toast(Say say, const std::string &arg)
    {
        zsdk::toast::Push(static_cast<int>(say), {}, { arg }, say == Say::Panel);
    }
}
