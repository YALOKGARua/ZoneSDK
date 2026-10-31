#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace zsdk
{
    void Log(const char *shape, ...);
    void Note(int level, const char *shape, ...);
    void Rare(const char *tag, const char *shape, ...);
    void Flush();
    void Urgent();

    namespace log
    {
        void Open(const std::filesystem::path &file, const std::filesystem::path &rolled);
        std::filesystem::path Path();
        int Level();
        void SetLevel(int level);
        void Hushed(uint64_t now);
        bool Due(uint64_t now);
        size_t Pending();
        size_t Dropped();
        void Roll(const std::filesystem::path &file, const std::filesystem::path &rolled);
        std::string Masked(std::string_view utf8);
        std::string Shown(const std::filesystem::path &path);
    }
}
