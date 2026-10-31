#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace zsdk::report
{
    inline constexpr size_t kLogLines = 80;
    inline constexpr size_t kTailBytes = 64u << 10;

    std::string Version(void *module);
    std::string Text(void *reshade);
    std::filesystem::path File();
    bool Save(const std::string &text);
}
