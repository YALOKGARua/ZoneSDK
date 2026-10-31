#pragma once

#include <filesystem>
#include <vector>

namespace zsdk::paths
{
    struct Roots
    {
        std::filesystem::path exe;
        std::filesystem::path local;
    };

    Roots Current();

    std::filesystem::path GameRoot(const Roots &roots);
    std::filesystem::path Mods(const Roots &roots);
    std::filesystem::path Saved(const Roots &roots);
    std::filesystem::path Logs(const Roots &roots);
    std::filesystem::path AppliedSettings(const Roots &roots);
    std::filesystem::path SteamManifest(const Roots &roots);
    std::vector<std::filesystem::path> SaveGamesData(const Roots &roots);
    bool PlayerSave(const std::filesystem::path &file);

    std::filesystem::path GameRoot();
    std::filesystem::path Mods();
    std::filesystem::path Saved();
    std::filesystem::path Logs();
    std::filesystem::path AppliedSettings();
    std::filesystem::path SteamManifest();
    std::vector<std::filesystem::path> SaveGamesData();
}
