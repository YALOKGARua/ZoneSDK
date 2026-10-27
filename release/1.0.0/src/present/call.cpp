#include <zsdk/present.h>

#include "../detail/engine_core.h"
#include "../detail/ledger_raw.h"
#include "../detail/runtime.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace zsdk
{
    namespace core = detail::engine;

    namespace
    {
        std::unordered_map<std::string, Fn> fns;
        std::unordered_map<std::string, uint32_t> tried;
        std::array<ue::Mark, census::kSlots> statics{};
        std::unordered_set<std::string> safe;
        std::atomic<bool> halted{ false };

        bool Pumped()
        {
            return zsdk::thread::OnGame() && zsdk::detail::thread::InPump();
        }

        bool Pointer(const std::string &type)
        {
            return type == "ObjectProperty" || type == "ClassProperty" || type == "InterfaceProperty";
        }

        bool Built(const std::string &build)
        {
            return !build.empty() && build != "?";
        }

        bool Adopt(Fn &fn, const void *function)
        {
            fn.function = function;
            fn.owner = ue::OuterOf(function);
            fn.sign = {};
            ue::Analyze(function, fn.sign);
            bool fits = fn.owner && fn.sign.bytes >= 0 && fn.sign.bytes <= 2048;
            for (const ue::Param &p : fn.sign.params)
                if (p.at < 0 || p.size < 0 || p.at + p.size > 2048) fits = false;
            fn.missing = !fits;
            return fits;
        }

        const void *Match(const char *owner, const char *name, const void *target)
        {
            const std::vector<const void *> found = core::Exact(name);
            if (found.empty()) return nullptr;
            for (const void *one : found)
                if (ue::Label(ue::OuterOf(one)) == owner) return one;
            if (!target) return nullptr;
            const void *type = ue::ClassOf(target);
            for (const void *one : found)
            {
                const void *home = ue::OuterOf(one);
                if (home && (ue::Kin(type, home) || core::Facet(target, one) != target)) return one;
            }
            return nullptr;
        }

        const void *Statics(const char *owner, const Fn &fn)
        {
            const CensusSpec &spec = app::Get().census;
            for (int i = 0; i < spec.staticCount && i < static_cast<int>(census::kSlots); ++i)
            {
                if (std::string_view(owner) != spec.statics[i]) continue;
                ue::Mark &m = statics[static_cast<size_t>(i)];
                if (m.actor && ue::Ours(m)) return m.actor;
                m = {};
                const auto snap = census::Current();
                const void *cdo = snap ? snap->statics[static_cast<size_t>(i)] : nullptr;
                if (cdo && !ue::Doomed(cdo) && ue::Kin(ue::ClassOf(cdo), fn.owner))
                {
                    m = ue::Seal(cdo);
                    Log("static host %s -> %s", owner, ue::Label(cdo).c_str());
                    return cdo;
                }
                break;
            }
            for (int i = 0; i < spec.hostCount && i < static_cast<int>(census::kSlots); ++i)
            {
                if (std::string_view(owner) != spec.hosts[i]) continue;
                const void *host = census::Host(static_cast<size_t>(i));
                if (host && ue::Kin(ue::ClassOf(host), fn.owner)) return host;
                break;
            }
            return ue::Host(fn.owner);
        }

        void Again(Fn &fn, const std::string &key, const char *owner, const char *name, const void *target)
        {
            const uint32_t epoch = core::BookEpoch();
            uint32_t &last = tried[key];
            if (!fn.missing || last == epoch) return;
            last = epoch;
            const void *function = Match(owner, name, target);
            if (function && Adopt(fn, function)) Log("function %s found after the function book was refreshed, %d bytes", key.c_str(), fn.sign.bytes);
        }
    }

    namespace detail
    {
        void Gate(const std::string &key, bool marked)
        {
            if (zsdk::thread::Stopping() || core::Dusked()) throw Refused("engine calls are stopped");
            if (!Pumped()) throw Refused("refused off-thread engine call");
            if (halted) throw Refused("engine calls are halted for this session");
            if (marked && ledger::Blocked(key)) throw Refused("disabled after a crash: " + key);
        }

        void Halt()
        {
            halted = true;
        }

        bool Halted()
        {
            return halted.load();
        }

        namespace call
        {
            bool Fits(const void *on, const void *owner, const void *function)
            {
                return ue::Kin(ue::ClassOf(on), owner) || (function && core::Facet(on, function) != on);
            }

            void Aim(const std::string &key, const void *on, bool checked, const void *owner, const void *function)
            {
                if (!on) throw Refused("host not ready: " + key);
                if (checked && ue::Doomed(on)) throw Refused(key + " refused for a doomed object");
                if (!Fits(on, owner, function)) throw std::runtime_error(key + " refused for " + ue::Label(ue::ClassOf(on)));
            }

            void Pass(const std::string &key, const ue::Param &param, const void *source, size_t bytes)
            {
                if (param.type == "StrProperty" && (param.flags & (core::kOutParm | core::kReturnParm)) != 0)
                    throw Refused("parameter " + param.name + " of " + key + " is an FString the game writes back, so no text is lent into it");
                if (!app::Get().policy.verifiedOnly || !source || bytes < sizeof(void *) || !Pointer(param.type)) return;
                const void *thing = nullptr;
                std::memcpy(&thing, source, sizeof(thing));
                if (thing && ue::Doomed(thing)) throw Refused("parameter " + param.name + " of " + key + " refused for a doomed object");
            }
        }
    }

    Fn &Lookup(const char *owner, const char *name, const void *target)
    {
        if (!Pumped()) throw Refused("a function lookup off the game thread pump was refused");
        const bool widen = app::Get().policy.widen;
        const std::string key = verbs::Key(owner, name);
        const auto found = fns.find(key);
        if (found != fns.end())
        {
            Fn &known = found->second;
            if (known.missing && target && widen && !known.widened)
            {
                known.widened = true;
                const void *function = Match(owner, name, target);
                if (function && Adopt(known, function)) Log("function %s resolved on %s, %d bytes", key.c_str(), ue::Label(known.owner).c_str(), known.sign.bytes);
            }
            return known;
        }
        Fn fn;
        fn.widened = widen && target != nullptr;
        tried[key] = core::BookEpoch();
        const void *function = Match(owner, name, widen ? target : nullptr);
        if (function) Adopt(fn, function);
        return fns.emplace(key, std::move(fn)).first->second;
    }

    Call::Call(std::span<const verbs::Spec> table, int verb, const void *on)
    {
        if (!Pumped()) throw Refused("a call was built off the game thread pump");
        if (verb < 0 || static_cast<size_t>(verb) >= table.size()) throw std::runtime_error("verb index out of range");
        const verbs::Spec &spec = table[static_cast<size_t>(verb)];
        const App &a = app::Get();
        if (a.policy.verifiedOnly && (table.data() != a.specs.data() || !verbs::Ready(verb))) throw Refused("unverified function " + verbs::KeyOf(spec));
        risky = spec.mark == verbs::Mark::Risky;
        Bind(spec.owner, spec.name, on, false);
    }

    Call::Call(const char *owner, const char *name, const void *on)
    {
        if (!Pumped()) throw Refused("a call was built off the game thread pump");
        Bind(owner, name, on, true);
    }

    void Call::Bind(const char *owner, const char *name, const void *on, bool fresh)
    {
        key = verbs::Key(owner, name);
        fn = &Lookup(owner, name, on);
        if (fresh) Again(*fn, key, owner, name, on);
        if (fn->missing) throw std::runtime_error("missing function " + key);
        const bool named = on != nullptr;
        if (!on) on = Statics(owner, *fn);
        detail::call::Aim(key, on, named && app::Get().policy.verifiedOnly, fn->owner, fn->function);
        target = on;
    }

    const ue::Param *Call::Find(const char *name) const
    {
        for (const ue::Param &p : fn->sign.params)
            if (p.name == name) return &p;
        if (app::Get().policy.spelling == Spelling::Tolerant)
            for (const ue::Param &p : fn->sign.params)
                if (text::Same(p.name, name))
                {
                    const std::string tag = key + "/" + name;
                    Rare(tag.c_str(), "call: parameter %s of %s is spelled %s here", name, key.c_str(), p.name.c_str());
                    return &p;
                }
        return nullptr;
    }

    const uint8_t *Call::Data() const
    {
        return block.data();
    }

    bool Call::Has(const char *name) const
    {
        return Find(name) != nullptr;
    }

    const ue::Param &Call::Parameter(const char *name) const
    {
        const ue::Param *p = Find(name);
        if (!p) throw std::runtime_error(std::string("missing parameter ") + name + " of " + ue::Label(fn->function));
        return *p;
    }

    void Call::Put(const char *name, const void *source, size_t bytes)
    {
        const ue::Param &p = Parameter(name);
        if (p.size != static_cast<int32_t>(bytes)) throw std::runtime_error(std::string("unexpected size for ") + name);
        detail::call::Pass(key, p, source, bytes);
        std::memcpy(block.data() + p.at, source, bytes);
    }

    std::wstring Call::CopyText(const char *name) const
    {
        const ue::Param &p = Parameter(name);
        if (p.size != static_cast<int32_t>(sizeof(ue::Lent)) || p.type != "StrProperty") throw std::runtime_error(std::string("not a text parameter: ") + name);
        return ue::CopyText(block.data() + p.at);
    }

    void Call::Wide(const char *name, int64_t value)
    {
        const ue::Param &p = Parameter(name);
        if (p.size < 1 || p.size > 8) throw std::runtime_error(std::string("odd size for ") + name);
        std::memcpy(block.data() + p.at, &value, static_cast<size_t>(p.size));
    }

    int64_t Call::Widened(const char *name) const
    {
        const ue::Param &p = Parameter(name);
        if (p.size < 1 || p.size > 8) throw std::runtime_error(std::string("odd size for ") + name);
        int64_t out = 0;
        std::memcpy(&out, block.data() + p.at, static_cast<size_t>(p.size));
        return out;
    }

    void Call::CopyFrom(const char *name, const Call &source, const char *sourceName)
    {
        const ue::Param &p = source.Parameter(sourceName);
        Put(name, source.block.data() + p.at, static_cast<size_t>(p.size));
    }

    const std::string &Call::Key() const
    {
        return key;
    }

    const Fn &Call::Function() const
    {
        return *fn;
    }

    const void *Call::Target() const
    {
        return target;
    }

    bool Call::Risky() const
    {
        return risky;
    }

    namespace
    {
        void Run(const std::string &key, bool marked, const Fn &fn, const void *target, uint8_t *block)
        {
            detail::Gate(key, marked);
            const Policy &policy = app::Get().policy;
            const std::string build = exe::Short();
            const bool first = marked && safe.count(key) == 0 && (!Built(build) || ledger::Proven(key) != build) && !ledger::Preloaded(key);
            const void *facet = core::Facet(target, fn.function);
            if (first && !ledger::raw::Arm(key) && policy.strictMarks)
            {
                ledger::raw::Settle();
                Rare("crash marker", "safety: the crash marker for %s could not be written, the call is refused", key.c_str());
                throw Refused("crash marker unavailable: " + key);
            }
            alignas(16) uint8_t fruit[64]{};
            core::Rode rode = core::Rode::Refused;
            try
            {
                rode = core::Ride(fn.function, facet, block, fruit);
            }
            catch (...)
            {
                if (first) ledger::raw::Settle();
                throw;
            }
            if (rode == core::Rode::Refused)
            {
                if (first) ledger::raw::Settle();
                throw Refused("refused off-thread engine call");
            }
            if (rode == core::Rode::Faulted)
            {
                if (marked) ledger::raw::Disable(key);
                if (policy.halt == Halt::Session) detail::Halt();
                throw Fault("native call failed: " + ue::Label(fn.function));
            }
            if (first) ledger::raw::Prove(key, build);
            if (marked) safe.insert(key);
        }
    }

    void Fire(Call &call)
    {
        const bool marked = app::Get().policy.marks == Marks::All || call.risky;
        Run(call.key, marked, *call.fn, call.target, call.block.data());
    }

    void Fire(Call &call, std::string_view key)
    {
        Run(std::string(key), true, *call.fn, call.target, call.block.data());
    }
}
