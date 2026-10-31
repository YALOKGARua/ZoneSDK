#pragma once

#include "scan.h"

#include <cstdint>
#include <string>

namespace hardrules
{
    enum class Kind : uint8_t { Native, Worker, Fresh };

    struct Rule
    {
        const char *pattern;
        const char *word;
        Kind kind;
    };

    inline constexpr Rule kRules[] = {
        { "Ride (", "Ride(", Kind::Native },
        { "ProcessEvent*", "ProcessEvent", Kind::Native },
        { "__try*", "__try", Kind::Native },
        { "__except*", "__except", Kind::Native },
        { "CreateThread*", "CreateThread", Kind::Worker },
        { "std :: thread*", "std::thread", Kind::Worker },
        { "SetWindowsHookEx*", "SetWindowsHookEx", Kind::Native },
        { "VirtualProtect*", "VirtualProtect", Kind::Native },
        { "WriteProcessMemory*", "WriteProcessMemory", Kind::Native },
        { "_InterlockedOr*", "_InterlockedOr", Kind::Native },
        { "native_bindings ::", "native_bindings::", Kind::Native },
        { "detail :: engine*", "detail::engine", Kind::Native },
        { "detail /", "detail/", Kind::Native },
        { "zsdk :: detail*", "zsdk::detail", Kind::Native },
        { "CreateTimerQueueTimer", "CreateTimerQueueTimer", Kind::Fresh },
        { "QueueUserWorkItem", "QueueUserWorkItem", Kind::Fresh },
        { "TrySubmitThreadpoolCallback", "TrySubmitThreadpoolCallback", Kind::Fresh },
        { "_beginthread", "_beginthread", Kind::Fresh },
        { "_beginthreadex", "_beginthreadex", Kind::Fresh },
        { "std :: async", "std::async", Kind::Fresh },
        { "std :: jthread", "std::jthread", Kind::Fresh },
        { "CreateRemoteThread", "CreateRemoteThread", Kind::Fresh },
        { "CreateRemoteThreadEx", "CreateRemoteThreadEx", Kind::Fresh },
        { "SetTimer", "SetTimer", Kind::Fresh },
    };

    inline std::string Hint(const Rule &rule)
    {
        if (rule.kind == Kind::Native) return std::string();
        return ": background work runs through zsdk::job::Every on the SDK worker, never on an own thread, timer or pool callback";
    }

    inline std::string Refusal(const Rule &rule, const std::string &where)
    {
        return std::string("no ") + rule.word + " in " + where + Hint(rule);
    }

    inline std::string Notice(const Rule &rule, const std::string &where)
    {
        return std::string("hardrules: ") + rule.word + " in " + where + Hint(rule);
    }

    inline bool Hits(const std::string &body, const Rule &rule)
    {
        return scan::Uses(scan::Tokens(scan::Lex(body).code), rule.pattern);
    }
}
