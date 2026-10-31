#include <zsdk/present.h>

#include <atomic>
#include <shared_mutex>
#include <string>
#include <vector>

namespace zsdk::verbs
{
    namespace
    {
        std::shared_mutex gate;
        std::vector<Check> checks;
        std::vector<std::string> notes;
        Report last;
        std::atomic<bool> primed{ false };
    }

    std::string Key(const char *owner, const char *name)
    {
        return std::string(owner) + "." + name;
    }

    std::string KeyOf(const Spec &spec)
    {
        return Key(spec.owner, spec.name);
    }

    bool TypeFits(const std::string &type, const char *types)
    {
        if (!types) return true;
        const std::string_view list(types);
        size_t from = 0;
        while (from <= list.size())
        {
            size_t bar = list.find('|', from);
            if (bar == std::string_view::npos) bar = list.size();
            if (list.substr(from, bar - from) == type) return true;
            from = bar + 1;
        }
        return false;
    }

    Check Verify(const ue::Sign &sign, bool missing, bool found, const Spec &spec, bool tolerant, std::string &note, std::string &spelled)
    {
        spelled.clear();
        if (missing)
        {
            note = found ? "rejected" : "missing";
            return Check::Bad;
        }
        std::string seen;
        for (const ue::Param &p : sign.params) seen += text::Format(" %s:%s@%d/%d", p.name.c_str(), p.type.c_str(), p.at, p.size);
        if (spec.count >= 0 && static_cast<int>(sign.params.size()) != spec.count)
        {
            note = text::Format("%d parameters, wants %d, found:%s", static_cast<int>(sign.params.size()), static_cast<int>(spec.count), seen.empty() ? " none" : seen.c_str());
            return Check::Bad;
        }
        if (spec.bytes >= 0 && sign.bytes != spec.bytes)
        {
            note = text::Format("parameter block %d bytes, wants %d", sign.bytes, spec.bytes);
            return Check::Bad;
        }
        for (const Want &want : spec.params)
        {
            if (!want.name) break;
            const ue::Param *hit = nullptr;
            for (const ue::Param &p : sign.params)
                if (p.name == want.name) hit = &p;
            if (!hit && tolerant)
            {
                for (const ue::Param &p : sign.params)
                    if (!hit && text::Same(p.name, want.name)) hit = &p;
                if (hit) spelled += text::Format("%sparameter %s is spelled %s here", spelled.empty() ? "" : "; ", want.name, hit->name.c_str());
            }
            if (!hit)
            {
                note = std::string("no parameter ") + want.name + ", found:" + (seen.empty() ? std::string(" none") : seen);
                return Check::Bad;
            }
            if (!TypeFits(hit->type, want.types) || (want.at >= 0 && hit->at != want.at) || (want.size > 0 && hit->size != want.size))
            {
                note = text::Format("parameter %s wants %s@%d/%d, found:%s", want.name, want.types ? want.types : "any", want.at, want.size, seen.c_str());
                return Check::Bad;
            }
        }
        note = seen.empty() ? " no parameters" : seen;
        return Check::Ok;
    }

    Report Prime(std::span<const Spec> specs)
    {
        const bool tolerant = app::Get().policy.spelling == Spelling::Tolerant;
        Report r;
        std::vector<Check> made(specs.size(), Check::Unknown);
        std::vector<std::string> said(specs.size());
        std::vector<std::string> failures;
        std::vector<std::string> spellings;
        for (size_t i = 0; i < specs.size(); ++i)
        {
            const Spec &spec = specs[i];
            Fn &fn = Lookup(spec.owner, spec.name);
            std::string note;
            std::string spelled;
            const Check check = Verify(fn.sign, fn.missing, fn.function != nullptr, spec, tolerant, note, spelled);
            made[i] = check;
            said[i] = note;
            if (spec.required) ++r.requiredAll;
            else ++r.optionalAll;
            if (check == Check::Ok)
            {
                if (spec.required) ++r.requiredOk;
                else ++r.optionalOk;
                if (!spelled.empty()) spellings.push_back(text::Format("verb: %s.%s %s", spec.owner, spec.name, spelled.c_str()));
            }
            else
            {
                failures.push_back(text::Format("verb: %s.%s %s %s%s", spec.owner, spec.name, fn.missing && !fn.function ? "missing" : "bad", note.c_str(), spec.required ? "" : " (optional)"));
                r.missing += (r.missing.empty() ? "" : ", ") + Key(spec.owner, spec.name);
            }
            zsdk::Note(2, "verb %s.%s %s:%s", spec.owner, spec.name, check == Check::Ok ? "ok" : "BAD", note.c_str());
        }
        r.coreReady = r.requiredOk == r.requiredAll;
        Log("verbs: %d/%d required, %d/%d optional, missing %s", r.requiredOk, r.requiredAll, r.optionalOk, r.optionalAll, r.missing.empty() ? "none" : r.missing.c_str());
        for (const std::string &line : failures) Log("%s", line.c_str());
        for (const std::string &line : spellings) Log("%s", line.c_str());
        {
            std::unique_lock<std::shared_mutex> hold(gate);
            checks.swap(made);
            notes.swap(said);
            last = r;
        }
        primed = true;
        return r;
    }

    Report Last()
    {
        std::shared_lock<std::shared_mutex> hold(gate);
        return last;
    }

    bool Ready(int verb)
    {
        std::shared_lock<std::shared_mutex> hold(gate);
        return verb >= 0 && static_cast<size_t>(verb) < checks.size() && checks[static_cast<size_t>(verb)] == Check::Ok;
    }

    std::string Note(int verb)
    {
        std::shared_lock<std::shared_mutex> hold(gate);
        return verb >= 0 && static_cast<size_t>(verb) < notes.size() ? notes[static_cast<size_t>(verb)] : std::string();
    }

    bool Primed()
    {
        return primed.load();
    }
}
