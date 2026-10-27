#include <zsdk/present.h>

#include "fieldwatch.h"

#include <mutex>
#include <string>

namespace zfieldwatch::core
{
    namespace
    {
        using zsdk::Log;

        Settings cfg;
        uint64_t revision = 0;
        uint64_t readAt = 0;
        Reading fov;
        Reading locked;
        std::mutex gate;
        Status shown;
        std::mutex judging;
        Board judged;

        const char *SetWord(zsdk::guard::State state)
        {
            return state == zsdk::guard::State::Ok ? "on" : state == zsdk::guard::State::Off ? "off" : "pending";
        }

        const void *Camera()
        {
            const zsdk::world::Grip &w = zsdk::world::Hold();
            if (!w.stable || !zsdk::ue::Here(w.lens, w.lensType)) return nullptr;
            return zsdk::ue::Inherits(w.lensType, fields::kDefaultFov.reflectedClass) ? w.lens : nullptr;
        }

        void Tell(const zsdk::guard::Field &field, const Reading &before, const Reading &now)
        {
            if (now.verdict == before.verdict) return;
            if (now.verdict == Verdict::Read) Log("fieldwatch: %s reads %.1f", field.name, now.value);
            else if (now.verdict == Verdict::Waiting) zsdk::Note(2, "fieldwatch: %s %s", field.name, watch::Word(now.verdict));
            else Log("fieldwatch: %s %s", field.name, watch::Word(now.verdict));
        }
    }

    void Boot()
    {
        if (!zsdk::job::Every(kJudgeJob, kJudgeMs, &Judge)) Log("fieldwatch: the verdict job %s was refused", kJudgeJob);
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
        const zsdk::guard::Field &f = fields::kDefaultFov;
        const bool same = zsdk::guard::Confirm(f);
        watch::Checked(true);
        Log("fieldwatch: %s at +0x%X %s %s.%s in the game reflection", f.name, f.at, same ? "matches" : "does not match", f.reflectedClass, f.reflectedProperty);
        for (const std::string &key : zsdk::ledger::Booted())
            if (key == kDefaultKey || key == kLockedKey) Log("fieldwatch: %s was disabled at this boot, the game ended while its crash marker was set; Try again in the tab clears it", key.c_str());
    }

    void Step(uint64_t now)
    {
        if (zsdk::Age(now, readAt) < kReadMs) return;
        readAt = now;
        const Reading lastFov = fov;
        const Reading lastLocked = locked;
        if (!cfg.enabled)
        {
            fov = Reading{};
            locked = Reading{};
            return;
        }
        const void *camera = Camera();
        const zsdk::guard::Field &lock = watch::Locked();
        fov = watch::Read(camera, fields::kDefaultFov, kDefaultKey);
        locked = watch::Read(camera, lock, kLockedKey);
        Tell(fields::kDefaultFov, lastFov, fov);
        Tell(lock, lastLocked, locked);
    }

    bool Hot()
    {
        return false;
    }

    void Publish(uint64_t)
    {
        const zsdk::world::Grip &w = zsdk::world::Hold();
        std::lock_guard<std::mutex> hold(gate);
        shown.stable = w.stable;
        shown.enabled = cfg.enabled;
        shown.wait = w.wait ? w.wait : "";
        shown.fov.reading = fov;
        shown.locked.reading = locked;
    }

    void Turn(const zsdk::world::Turn &turn)
    {
        fov = Reading{};
        locked = Reading{};
        zsdk::Note(1, "fieldwatch: world turn %u (%s), the readings start again", turn.epoch, turn.why);
    }

    bool Complete(const zsdk::census::Snapshot &snapshot)
    {
        return snapshot.gi && snapshot.statics[0] && snapshot.statics[1];
    }

    void Judge(uint64_t)
    {
        const Board b = watch::Look();
        if (b.build == Build::Unchecked) return;
        {
            std::lock_guard<std::mutex> hold(judging);
            if (b == judged) return;
            judged = b;
        }
        const std::string build = b.build == Build::Known   ? "game build " + b.id + " is known to this ZoneSDK"
                                  : b.build == Build::Other ? "game build " + b.id + " is not known to this ZoneSDK"
                                                            : std::string("the game build is not supported, ZoneSDK did not bind the engine");
        Log("fieldwatch: %s; guard set %s %s; %s %s, %s %s, %s %s", build.c_str(), b.camera ? b.camera->name : fields::kCameraName, SetWord(b.set), fields::kDefaultFov.name, watch::Word(b.fov),
            watch::Locked().name, watch::Word(b.locked), fields::kViewFov.name, watch::Word(b.view));
    }

    Status Snapshot()
    {
        Status s;
        {
            std::lock_guard<std::mutex> hold(gate);
            s = shown;
        }
        s.phase = zsdk::thread::State(s.stable);
        s.board = watch::Look();
        s.fov = watch::Live(fields::kDefaultFov, kDefaultKey, s.fov.reading);
        s.locked = watch::Live(watch::Locked(), kLockedKey, s.locked.reading);
        return s;
    }
}
