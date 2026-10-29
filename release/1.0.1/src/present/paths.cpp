#include <zsdk/present.h>

#include <windows.h>

#include <algorithm>
#include <string>
#include <system_error>

namespace zsdk::paths
{
    namespace
    {
        constexpr DWORD kMostChars = 32768;
        constexpr size_t kMostSubs = 64;

        std::filesystem::path Module()
        {
            std::wstring buffer(MAX_PATH, L'\0');
            for (;;)
            {
                const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (n == 0) return {};
                if (n < buffer.size())
                {
                    buffer.resize(n);
                    return buffer;
                }
                if (buffer.size() >= kMostChars) return {};
                buffer.resize(std::min<size_t>(buffer.size() * 2, kMostChars));
            }
        }

        std::filesystem::path Env(const wchar_t *name)
        {
            const DWORD need = GetEnvironmentVariableW(name, nullptr, 0);
            if (need == 0 || need > kMostChars) return {};
            std::wstring buffer(need, L'\0');
            const DWORD n = GetEnvironmentVariableW(name, buffer.data(), need);
            if (n == 0 || n >= need) return {};
            buffer.resize(n);
            return buffer;
        }

        bool Named(const std::filesystem::path &folder, const wchar_t *name)
        {
            return _wcsicmp(folder.filename().c_str(), name) == 0;
        }
    }

    Roots Current()
    {
#if defined(ZSDK_TESTS)
        const std::filesystem::path home = Env(L"ZSDK_TEST_HOME");
        if (!home.empty()) return Roots{ home / L"steamapps" / L"common" / L"Game" / L"Stalker2" / L"Binaries" / L"Win64" / L"Stalker2-Win64-Shipping.exe", home };
#endif
        return Roots{ Module(), Env(L"LOCALAPPDATA") };
    }

    std::filesystem::path GameRoot(const Roots &roots)
    {
        if (roots.exe.empty()) return {};
        const std::filesystem::path win = roots.exe.parent_path();
        const std::filesystem::path bin = win.parent_path();
        if (!Named(win, L"Win64") || !Named(bin, L"Binaries")) return {};
        const std::filesystem::path game = bin.parent_path();
        return game == bin || game.filename().empty() ? std::filesystem::path() : game;
    }

    std::filesystem::path Mods(const Roots &roots)
    {
        const std::filesystem::path game = GameRoot(roots);
        return game.empty() ? std::filesystem::path() : game / L"Content" / L"Paks" / L"~mods";
    }

    std::filesystem::path Saved(const Roots &roots)
    {
        return roots.local.empty() ? std::filesystem::path() : roots.local / L"Stalker2" / L"Saved";
    }

    std::filesystem::path Logs(const Roots &roots)
    {
        const std::filesystem::path saved = Saved(roots);
        return saved.empty() ? std::filesystem::path() : saved / L"Logs";
    }

    std::filesystem::path AppliedSettings(const Roots &roots)
    {
        const std::filesystem::path saved = Saved(roots);
        return saved.empty() ? std::filesystem::path() : saved / L"GameSettings" / L"AppliedSettingsWin64.cfg";
    }

    std::filesystem::path SteamManifest(const Roots &roots)
    {
        const std::filesystem::path game = GameRoot(roots);
        if (game.empty()) return {};
        const std::filesystem::path common = game.parent_path().parent_path();
        const std::filesystem::path steamapps = common.parent_path();
        if (!Named(common, L"common") || !Named(steamapps, L"steamapps")) return {};
        return steamapps / L"appmanifest_1643320.acf";
    }

    std::vector<std::filesystem::path> SaveGamesData(const Roots &roots)
    {
        std::vector<std::filesystem::path> out;
        const std::filesystem::path saved = Saved(roots);
        if (saved.empty()) return out;
        std::error_code code;
        for (std::filesystem::directory_iterator it(saved, std::filesystem::directory_options::skip_permission_denied, code), end; !code && it != end && out.size() < kMostSubs; it.increment(code))
        {
            std::error_code kind;
            if (!it->is_directory(kind) || kind) continue;
            const std::filesystem::path data = it->path() / L"SaveGames" / L"Data";
            if (std::filesystem::is_directory(data, kind) && !kind) out.push_back(data);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    bool PlayerSave(const std::filesystem::path &file)
    {
        const std::filesystem::path data = file.parent_path();
        if (_wcsicmp(file.extension().c_str(), L".sav") != 0 || !Named(data, L"Data") || !Named(data.parent_path(), L"SaveGames")) return false;
        const std::wstring stem = file.stem().wstring();
        return !stem.empty() && _wcsicmp(stem.c_str(), L"CampaignsSave") != 0 && _wcsicmp(stem.c_str(), L"AnalyticsData") != 0;
    }

    std::filesystem::path GameRoot()
    {
        return GameRoot(Current());
    }

    std::filesystem::path Mods()
    {
        return Mods(Current());
    }

    std::filesystem::path Saved()
    {
        return Saved(Current());
    }

    std::filesystem::path Logs()
    {
        return Logs(Current());
    }

    std::filesystem::path AppliedSettings()
    {
        return AppliedSettings(Current());
    }

    std::filesystem::path SteamManifest()
    {
        return SteamManifest(Current());
    }

    std::vector<std::filesystem::path> SaveGamesData()
    {
        return SaveGamesData(Current());
    }
}
