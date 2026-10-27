#include <zsdk/present.h>

#include "fieldwatch.h"

#include <atomic>
#include <cmath>

namespace zfieldwatch::watch
{
    namespace
    {
        namespace guard = zsdk::guard;

        std::atomic<bool> reflection{ false };

        bool Unbound()
        {
            return zsdk::thread::State(false) == zsdk::Phase::Failed;
        }
    }

    void Checked(bool done)
    {
        reflection = done;
    }

    const guard::Field &On(std::span<const guard::Field *const> fields)
    {
        for (const guard::Field *field : fields)
            if (field->set && guard::Of(*field->set) == guard::State::Ok) return *field;
        return *fields.front();
    }

    const guard::Field &Locked()
    {
        return On(fields::kLockedFovs);
    }

    Verdict Gate(const guard::Field &field)
    {
        const bool rejected = Unbound();
        switch (field.proof)
        {
        case guard::Proof::Unproven: return Verdict::Unproven;
        case guard::Proof::Reflected: return guard::Usable(field) ? Verdict::Read : reflection.load() || rejected ? Verdict::Off : Verdict::Pending;
        default:
        {
            const guard::State state = field.set ? guard::Of(*field.set) : guard::State::Off;
            return state == guard::State::Ok ? Verdict::Read : state == guard::State::Pending && !rejected ? Verdict::Pending : Verdict::Off;
        }
        }
    }

    Reading Read(const void *object, const guard::Field &field, const char *key)
    {
        if (key && zsdk::ledger::Blocked(key)) return { .verdict = Verdict::Crashed };
        const Verdict gate = Gate(field);
        if (gate != Verdict::Read) return { .verdict = gate };
        if (!key) return { .verdict = Verdict::Unmarked };
        if (!object) return { .verdict = Verdict::Waiting };
        zsdk::ledger::Window window(key);
        if (window.Refused()) return { .verdict = zsdk::ledger::Blocked(key) ? Verdict::Crashed : Verdict::Unmarked };
        float value = 0.0f;
        const bool read = zsdk::mem::Read(zsdk::Addr(object), field, value);
        window.Close();
        if (!read) return { .verdict = Verdict::Waiting };
        if (!std::isfinite(value)) return { .verdict = Verdict::Off };
        return { .verdict = Verdict::Read, .value = value };
    }

    Watched Live(const guard::Field &field, const char *key, const Reading &last)
    {
        Watched w{ .reading = last };
        const Verdict gate = Gate(field);
        if (key && zsdk::ledger::Blocked(key)) w.reading = { .verdict = Verdict::Crashed };
        else if (gate != Verdict::Read) w.reading = { .verdict = gate };
        w.proven = key && zsdk::exe::Hashed() && zsdk::ledger::Proven(key) == zsdk::exe::Short();
        return w;
    }

    Board Look()
    {
        Board b;
        const bool rejected = Unbound();
        if (zsdk::exe::Hashed())
        {
            b.build = zsdk::exe::Known() ? Build::Known : Build::Other;
            b.id = zsdk::exe::Short();
        }
        else if (rejected) b.build = Build::Rejected;
        const guard::Field &lock = Locked();
        b.set = guard::Of(*lock.set);
        b.camera = b.set == guard::State::Ok ? lock.set : nullptr;
        if (!b.camera)
            for (const guard::Set *set : fields::kSets)
                if (guard::Of(*set) == guard::State::Pending) b.set = guard::State::Pending;
        if (rejected && b.set == guard::State::Pending) b.set = guard::State::Off;
        b.fov = Gate(fields::kDefaultFov);
        b.locked = Gate(lock);
        b.view = Gate(fields::kViewFov);
        return b;
    }

    const char *Word(Verdict verdict)
    {
        switch (verdict)
        {
        case Verdict::Pending: return "waits for its check";
        case Verdict::Waiting: return "waits for the player camera";
        case Verdict::Read: return "usable";
        case Verdict::Off: return "off";
        case Verdict::Crashed: return "disabled after a crash";
        case Verdict::Unmarked: return "skipped, no crash marker";
        default: return "unproven, never read";
        }
    }
}
