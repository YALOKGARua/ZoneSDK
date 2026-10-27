#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zsdk::text
{
    std::string Trim(std::string_view raw);
    std::vector<std::string> Split(std::string_view line, char mark);
    std::vector<std::string> Tokens(std::string_view line);
    std::vector<std::string> Lines(const std::string &body);

    std::wstring Wide(std::string_view utf8);
    std::string Narrow(std::wstring_view wide);

    uint32_t Fold(uint32_t point);
    uint32_t Raise(uint32_t point);
    std::string Lower(std::string_view utf8);
    std::string Upper(std::string_view utf8);
    std::string Latin(std::string_view utf8);
    bool Cyrillic(std::string_view utf8);
    std::vector<uint32_t> Points(std::string_view utf8);

    bool Starts(std::string_view text, std::string_view head);
    bool Ends(std::string_view text, std::string_view tail);
    bool Same(std::string_view left, std::string_view right);
    bool Color(std::string_view token, uint32_t &argb);

    std::string Format(const char *shape, ...);
    std::string Read(const std::string &path);
    bool Write(const std::string &path, const std::string &body);
}
