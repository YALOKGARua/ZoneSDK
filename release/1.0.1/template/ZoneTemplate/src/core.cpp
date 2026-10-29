#include <zsdk/present.h>

#include "template.h"

#include <cstdio>
#include <mutex>
#include <string>

namespace ztemplate::core
{
    namespace
    {
        using zsdk::Log;
        using zsdk::Note;

        Settings cfg;
        uint64_t revision = 0;
        std::mutex gate;
        Status shown;
        std::string located;
        std::string refusal;

        void Tell(Say say, const std::string &arg)
        {
            if (cfg.toasts) Toast(say, arg);
        }

        void Refuse(const char *why)
        {
            refusal = why;
            Log("template: refused %s", why);
            Tell(Say::Refused, why);
        }

        void Locate()
        {
            zsdk::world::Grip &w = zsdk::world::Hold();
            if (!cfg.enabled) return Refuse("why.disabled");
            if (!zsdk::verbs::Ready(kLocation)) return Refuse("why.verbs");
            if (!w.stable || !zsdk::ue::Here(w.body, w.bodyType)) return Refuse("why.world");
            if (zsdk::ledger::Blocked(kLocateKey)) return Refuse("why.crash");
            try
            {
                zsdk::Call c(kSpecs, kLocation, w.body);
                zsdk::Fire(c, kLocateKey);
                const zsdk::math::Vec3 at = c.Get<zsdk::math::Vec3>("ReturnValue");
                char text[96]{};
                std::snprintf(text, sizeof(text), "%.0f %.0f %.0f", at.x, at.y, at.z);
                located = text;
                refusal.clear();
                Log("template: located at %s in %s", text, w.worldName.c_str());
                Tell(Say::Located, located);
            }
            catch (const zsdk::Fault &error)
            {
                Log("template: %s", error.what());
                Refuse("why.failed");
            }
            catch (const std::exception &error)
            {
                Log("template: locate failed: %s", error.what());
                Refuse("why.failed");
            }
        }
    }

    void Refresh(bool again)
    {
        const uint64_t now = zsdk::ini::Revision();
        if (now == revision && !again) return;
        revision = now;
        cfg = zsdk::ini::Config<Settings>();
    }

    void Prime()
    {
        const zsdk::verbs::Report &r = zsdk::verbs::Last();
        if (!r.coreReady) Log("template: a required game function is missing, the locate call is off");
        for (const std::string &key : zsdk::ledger::Booted()) Tell(Say::Crashed, key);
    }

    void Step(uint64_t)
    {
    }

    bool Hot()
    {
        return false;
    }

    void Publish(uint64_t)
    {
        const zsdk::world::Grip &w = zsdk::world::Hold();
        std::lock_guard<std::mutex> hold(gate);
        shown.coreReady = zsdk::verbs::Last().coreReady;
        shown.stable = w.stable;
        shown.wait = w.wait ? w.wait : "";
        shown.located = located;
        shown.refusal = refusal;
    }

    void Turn(const zsdk::world::Turn &turn)
    {
        located.clear();
        Note(1, "template: world turn %u (%s)", turn.epoch, turn.why);
    }

    void Order(int order)
    {
        if (order == static_cast<int>(Order::Locate)) Locate();
    }

    const char *OrderName(int order)
    {
        return order == static_cast<int>(Order::Locate) ? "locate" : "unknown";
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

    void Toast(Say say, const std::string &arg)
    {
        zsdk::toast::Push(static_cast<int>(say), {}, { arg }, false);
    }
}
