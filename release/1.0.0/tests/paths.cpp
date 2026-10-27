#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <string>
#include <vector>

namespace
{
    using check::That;
    namespace paths = zsdk::paths;
    namespace fs = std::filesystem;

    fs::path Exe(const fs::path &install)
    {
        return install / "Stalker2" / "Binaries" / "Win64" / "Stalker2-Win64-Shipping.exe";
    }

    bool Made(const fs::path &folder)
    {
        std::error_code code;
        fs::create_directories(folder, code);
        return fs::is_directory(folder, code);
    }

    bool Put(const fs::path &file)
    {
        return zsdk::ini::WriteAtomic(file, "x");
    }

    void Shape(const fs::path &home)
    {
        const fs::path install = home / "steamapps" / "common" / "Game";
        const paths::Roots r{ Exe(install), home / "local" };
        That(paths::GameRoot(r) == install / "Stalker2", "the game root is the folder above Binaries\\Win64");
        That(paths::Mods(r) == install / "Stalker2" / "Content" / "Paks" / "~mods", "mods is Content\\Paks\\~mods under the game root");
        That(paths::Saved(r) == home / "local" / "Stalker2" / "Saved", "saved is Stalker2\\Saved under LOCALAPPDATA");
        That(paths::Logs(r) == home / "local" / "Stalker2" / "Saved" / "Logs", "logs is Saved\\Logs");
        That(paths::AppliedSettings(r) == home / "local" / "Stalker2" / "Saved" / "GameSettings" / "AppliedSettingsWin64.cfg", "the applied settings file is in Saved\\GameSettings");
        That(paths::SteamManifest(r) == home / "steamapps" / "appmanifest_1643320.acf", "the Steam manifest sits in steamapps next to common");
        const paths::Roots upper{ home / "STEAMAPPS" / "Common" / "Game" / "Stalker2" / "BINARIES" / "win64" / "Stalker2-Win64-Shipping.exe", home / "local" };
        That(paths::GameRoot(upper) == home / "STEAMAPPS" / "Common" / "Game" / "Stalker2" && paths::SteamManifest(upper) == home / "STEAMAPPS" / "appmanifest_1643320.acf", "folder names match without case");
        const paths::Roots loose{ home / "bin" / "Stalker2-Win64-Shipping.exe", home / "local" };
        That(paths::GameRoot(loose).empty() && paths::Mods(loose).empty() && paths::SteamManifest(loose).empty(), "an exe outside Binaries\\Win64 has no game root, mods or manifest");
        const paths::Roots other{ Exe(home / "Games" / "S2"), home / "local" };
        That(paths::GameRoot(other) == home / "Games" / "S2" / "Stalker2" && paths::SteamManifest(other).empty(), "a game outside steamapps\\common has a root but no Steam manifest");
        const paths::Roots none{};
        That(paths::GameRoot(none).empty() && paths::Mods(none).empty() && paths::Saved(none).empty() && paths::Logs(none).empty() && paths::AppliedSettings(none).empty() &&
                 paths::SteamManifest(none).empty() && paths::SaveGamesData(none).empty(),
             "empty roots give empty paths");
        const paths::Roots bare{ r.exe, fs::path() };
        That(paths::Saved(bare).empty() && paths::AppliedSettings(bare).empty() && paths::SaveGamesData(bare).empty() && !paths::Mods(bare).empty(), "no LOCALAPPDATA gives no saved paths and keeps the game paths");
    }

    void Saves(const fs::path &home)
    {
        const paths::Roots r{ Exe(home / "steamapps" / "common" / "Game"), home / L"\u041a\u043e\u0440\u0438\u0441\u0442\u0443\u0432\u0430\u0447" };
        const fs::path saved = paths::Saved(r);
        That(Made(saved / "STEAM" / "SaveGames" / "Data") && Made(saved / "EGS" / "SaveGames" / "Data") && Made(saved / "STEAM" / "SaveGames_fwbackup" / "test3" / "Data") &&
                 Made(saved / "Config") && Made(saved / "Logs") && Made(saved / "Lone" / "SaveGames"),
             "the fake Saved tree is made");
        That(Put(saved / "STEAM" / "SaveGames" / "Data" / "0ABA6A69.sav") && Put(saved / "STEAM" / "SaveGames" / "CampaignsSave.sav") && Put(saved / "STEAM" / "SaveGames" / "AnalyticsData.sav") &&
                 Put(saved / "SaveGames" / "Other.sav") && Put(saved / "Note.txt"),
             "the fake save files are made");
        const std::vector<fs::path> found = paths::SaveGamesData(r);
        That(found == std::vector<fs::path>{ saved / "EGS" / "SaveGames" / "Data", saved / "STEAM" / "SaveGames" / "Data" }, "only <sub>\\SaveGames\\Data folders are listed, sorted, " + std::to_string(found.size()));
        const fs::path data = saved / "STEAM" / "SaveGames" / "Data";
        That(paths::PlayerSave(data / "0ABA6A69.sav") && paths::PlayerSave(data / "B.SAV"), "a .sav in SaveGames\\Data is a player save");
        That(!paths::PlayerSave(saved / "STEAM" / "SaveGames" / "CampaignsSave.sav") && !paths::PlayerSave(saved / "STEAM" / "SaveGames" / "AnalyticsData.sav"), "the campaign and analytics files are not player saves");
        That(!paths::PlayerSave(data / "CampaignsSave.sav") && !paths::PlayerSave(data / "analyticsdata.sav"), "the campaign and analytics names are refused inside Data too");
        That(!paths::PlayerSave(data / "0ABA6A69.txt") && !paths::PlayerSave(data / ".sav") && !paths::PlayerSave(saved / "STEAM" / "SaveGames_fwbackup" / "test3" / "Data" / "x.sav") &&
                 !paths::PlayerSave(saved / "SaveGames" / "Other.sav"),
             "other files, backups and loose saves are not player saves");
        That(paths::SaveGamesData(paths::Roots{ r.exe, home / "missing" }).empty(), "a missing Saved folder lists nothing");
    }

    void Seam(const fs::path &home)
    {
        if (scan::Env("ZSDK_TEST_HOME").empty())
        {
            check::Skip("the ZSDK_TESTS root seam needs ZSDK_TEST_HOME");
            return;
        }
        const paths::Roots now = paths::Current();
        That(now.local == home && now.exe == Exe(home / "steamapps" / "common" / "Game"), "under ZSDK_TESTS the roots lie inside ZSDK_TEST_HOME");
        That(paths::GameRoot() == paths::GameRoot(now) && paths::Mods() == paths::Mods(now) && paths::Saved() == paths::Saved(now) && paths::Logs() == paths::Logs(now) &&
                 paths::AppliedSettings() == paths::AppliedSettings(now) && paths::SteamManifest() == paths::SteamManifest(now),
             "the plain calls use the current roots");
        That(Made(paths::Saved(now) / "STEAM" / "SaveGames" / "Data") && paths::SaveGamesData() == std::vector<fs::path>{ home / "Stalker2" / "Saved" / "STEAM" / "SaveGames" / "Data" }, "the plain save list reads the fake tree");
    }
}

int main()
{
    const fs::path home = scan::EnvPath("ZSDK_TEST_HOME", fs::temp_directory_path() / "zsdk-paths");
    Shape(home / "shape");
    Saves(home / "saves");
    Seam(home);
    return check::Finish("paths");
}
