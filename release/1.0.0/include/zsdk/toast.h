#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zsdk::toast
{
    inline constexpr uint64_t kLifeMs = 3000;
    inline constexpr uint64_t kFadeMs = 400;
    inline constexpr size_t kShown = 3;
    inline constexpr size_t kKept = 8;
    inline constexpr int kUnsupported = -1;

    struct Item
    {
        int kind = 0;
        std::string detail;
        std::vector<std::string> args;
        uint64_t born = 0;
    };

    void Push(int kind, const std::string &detail, const std::vector<std::string> &args, bool replaceKind);
    std::vector<Item> Live(uint64_t now);
    void Reset();
    bool Focused();
}
