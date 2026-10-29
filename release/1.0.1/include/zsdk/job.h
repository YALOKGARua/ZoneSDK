#pragma once

#include <cstdint>

namespace zsdk::job
{
    inline constexpr int kMost = 8;
    inline constexpr uint32_t kShortestMs = 100;
    inline constexpr uint32_t kSlowMs = 50;
    inline constexpr uint32_t kBudgetMs = 50;
    inline constexpr uint32_t kStallMs = 1000;

    bool Every(const char *name, uint32_t periodMs, void (*run)(uint64_t now));
    bool Stopping();
    bool Off(const char *name);
}
