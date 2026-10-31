#pragma once

#include <span>
#include <string_view>

namespace zsdk::registry
{
    struct Row
    {
        const char *id;
        const char *tag;
        float lane;
        const char *keys;
    };

    inline constexpr float kSpareLane = 0.75f;

    std::span<const Row> Rows();
    const Row *Find(std::string_view id);
    float Lane(std::string_view id, float fallback);
}
