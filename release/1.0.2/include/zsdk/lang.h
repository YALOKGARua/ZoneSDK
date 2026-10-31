#pragma once

#include "paths.h"

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zsdk::lang
{
    enum class Lang : uint8_t { Uk, En, Count };

    struct Row
    {
        const char *id;
        const char *text[static_cast<size_t>(Lang::Count)];
    };

    Lang Current();
    void Set(Lang lang);
    void Set(std::string_view code);
    const char *Code(Lang lang);
    bool Valid(std::string_view code);
    Lang Detect();
    Lang Detect(const paths::Roots &roots);
    const char *Text(std::string_view id);
    const char *Text(std::string_view id, Lang lang);
    bool Has(std::string_view id);
    std::vector<std::string_view> Ids();
    std::span<const Row> Defaults();
    std::string Format(std::string_view pattern, std::initializer_list<std::string> args);
    std::string Format(std::string_view pattern, const std::vector<std::string> &args);
    std::vector<int> Slots(std::string_view pattern);
    bool Russian(std::string_view utf8);
}
