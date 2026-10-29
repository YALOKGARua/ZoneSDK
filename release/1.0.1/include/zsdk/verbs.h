#pragma once

#include "ue.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace zsdk::verbs
{
    enum class Mark : uint8_t { Auto, Risky };

    enum class Check : uint8_t { Unknown, Ok, Bad };

    struct Want
    {
        const char *name;
        const char *types;
        int32_t at;
        int32_t size;
    };

    struct Spec
    {
        const char *owner;
        const char *name;
        bool required;
        Want params[12];
        Mark mark = Mark::Auto;
        int16_t count = -1;
        int32_t bytes = -1;
    };

    struct Report
    {
        int requiredOk = 0;
        int requiredAll = 0;
        int optionalOk = 0;
        int optionalAll = 0;
        std::string missing;
        bool coreReady = false;
    };

    constexpr bool Same(const char *a, const char *b)
    {
        while (*a && *a == *b)
        {
            ++a;
            ++b;
        }
        return *a == *b;
    }

    template<size_t N> constexpr int Counted(const Spec (&specs)[N], bool required)
    {
        int n = 0;
        for (const Spec &spec : specs)
            if (spec.name && spec.required == required) ++n;
        return n;
    }

    std::string Key(const char *owner, const char *name);
    std::string KeyOf(const Spec &spec);
    bool TypeFits(const std::string &type, const char *types);
    Check Verify(const ue::Sign &sign, bool missing, bool found, const Spec &spec, bool tolerant, std::string &note, std::string &spelled);
    Report Prime(std::span<const Spec> specs);
    Report Last();
    bool Ready(int verb);
    std::string Note(int verb);
    bool Primed();
}
