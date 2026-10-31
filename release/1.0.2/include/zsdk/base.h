#pragma once

#include <cstdint>
#include <limits>

namespace zsdk
{
    inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    double Ms();
    uint64_t Now();

    inline constexpr uint64_t Age(uint64_t now, uint64_t at)
    {
        return at > now ? 0 : now - at;
    }

    inline uintptr_t Addr(const void *thing)
    {
        return reinterpret_cast<uintptr_t>(thing);
    }
}
