#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace zsdk::ledger
{
    struct Leftover
    {
        bool clean = false;
        bool older = false;
        std::vector<std::string> closed;
        std::vector<std::string> fallen;
        std::vector<std::string> cleared;
    };

    inline constexpr size_t kSessionMost = 64;
    inline constexpr size_t kMarkMost = 256;

    bool Session(std::string_view id);
    std::string Seal(std::string_view body);
    std::string Mark(std::string_view session, std::string_view seal);
    bool Marked(std::string_view mark, std::string_view session, std::string_view seal);
    Leftover Judge(std::string_view mark, std::string_view session, std::string_view seal, std::string_view pending, const std::vector<std::string> &watch);
    bool Ended(bool gameThread, int runtimes, bool dropped);
}

namespace zsdk::ledger::raw
{
    bool Arm(const std::string &key);
    bool Prove(const std::string &key, const std::string &build);
    bool Settle();
    bool Disable(const std::string &key);
    void Freeze();
    void Leave();
}
