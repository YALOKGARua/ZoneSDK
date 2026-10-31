#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace zsdk::glyphs
{
    struct Run
    {
        int first;
        int count;
    };

    inline constexpr Run kRuns[] = {
        { 0x0020, 95 },
        { 0x00A0, 96 },
        { 0x02BC, 1 },
        { 0x0400, 96 },
        { 0x0490, 2 },
        { 0x2010, 24 },
        { 0x2116, 1 },
    };

    constexpr bool Drawable(uint32_t point)
    {
        for (const Run &run : kRuns)
            if (point >= static_cast<uint32_t>(run.first) && point < static_cast<uint32_t>(run.first + run.count)) return true;
        return false;
    }

    std::string Undrawn(std::string_view utf8);
}
