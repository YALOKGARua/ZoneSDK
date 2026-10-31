#pragma once

#include <cstdint>
#include <string>

namespace zsdk
{
    enum class Marks : uint8_t { All, Flagged };
    enum class Halt : uint8_t { Key, Session };
    enum class GoneFlags : uint8_t { Strict, Legacy };
    enum class Spelling : uint8_t { Tolerant, Strict };
    enum class CensusMode : uint8_t { Interval, OnDemand };
    enum class Knock : uint8_t { BusyIdle, Fixed, Configured };
    enum class WireRetry : uint8_t { Backoff, Fixed };
    enum class FileThread : uint8_t { Render };
    enum class ModRule : uint8_t { Alt, Exact };

    struct Policy
    {
        Marks marks = Marks::All;
        bool strictMarks = true;
        bool verifiedOnly = true;
        Halt halt = Halt::Key;
        bool haltOnSeh = true;
        uint32_t watchKept = 8;
        GoneFlags gone = GoneFlags::Strict;
        Spelling spelling = Spelling::Tolerant;
        bool widen = false;
        CensusMode census = CensusMode::Interval;
        Knock knock = Knock::BusyIdle;
        uint32_t knockMs = 100;
        uint32_t idleMs = 1000;
        WireRetry wire = WireRetry::Backoff;
        uint32_t wireMs = 2000;
        FileThread files = FileThread::Render;
        ModRule mods = ModRule::Alt;
        friend bool operator==(const Policy &, const Policy &) = default;
    };

    bool Legacy(const Policy &policy);
    std::string LegacyText(const Policy &policy);
}
