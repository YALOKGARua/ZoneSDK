#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <windows.h>

#include "../src/detail/runtime.h"

#include <atomic>
#include <chrono>
#include <regex>
#include <string>
#include <thread>

namespace
{
    using check::That;

    struct Grabbed
    {
        std::atomic<int> state{ 0 };
        std::thread holder;

        Grabbed(int which, int ms)
        {
            holder = std::thread([this, which, ms] {
                zsdk::detail::Grip(which, true);
                state = 1;
                const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
                while (state.load() != 2 && std::chrono::steady_clock::now() < until) Sleep(1);
                zsdk::detail::Grip(which, false);
            });
            while (state.load() != 1) Sleep(1);
        }

        ~Grabbed()
        {
            state = 2;
            holder.join();
        }
    };

    double Since(std::chrono::steady_clock::time_point from)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
    }

    void Leaving(const std::filesystem::path &file)
    {
        zsdk::Flush();
        double waited = 0;
        {
            Grabbed hold(0, 150);
            const auto from = std::chrono::steady_clock::now();
            zsdk::Log("waited for the log lock");
            waited = Since(from);
        }
        That(waited >= 100.0, "without the exit flag a log line waits for a busy log lock");
        zsdk::detail::Exit(true);
        That(zsdk::detail::Exiting(), "the process-exit flag is set");
        double logMs = 0, flushMs = 0;
        {
            Grabbed hold(0, 5000);
            const auto from = std::chrono::steady_clock::now();
            zsdk::Log("dropped while the log lock is held");
            zsdk::Note(0, "dropped note while the log lock is held");
            zsdk::Rare("exit.rare", "dropped rare while the log lock is held");
            logMs = Since(from);
            const auto flushFrom = std::chrono::steady_clock::now();
            zsdk::Flush();
            flushMs = Since(flushFrom);
        }
        That(logMs < 50.0 && flushMs < 50.0, "at the process exit Log, Note, Rare and Flush return in under 50 ms while another thread holds the log lock");
        double rareMs = 0;
        {
            Grabbed hold(1, 5000);
            const auto from = std::chrono::steady_clock::now();
            zsdk::Rare("exit.hush", "dropped rare while the hush lock is held");
            rareMs = Since(from);
            zsdk::Log("kept while the hush lock is held");
        }
        That(rareMs < 50.0, "at the process exit Rare returns in under 50 ms while another thread holds the hush lock");
        double heldMs = 0;
        {
            Grabbed hold(2, 5000);
            zsdk::Log("kept while the flush lock is held");
            const auto from = std::chrono::steady_clock::now();
            zsdk::Flush();
            heldMs = Since(from);
        }
        That(heldMs < 50.0, "at the process exit Flush returns in under 50 ms while another thread holds the flush lock");
        That(scan::Body(file).find("kept while the flush lock is held") == std::string::npos, "a flush that finds its lock busy writes nothing");
        zsdk::Flush();
        const std::string body = scan::Body(file);
        That(body.find("waited for the log lock") != std::string::npos && body.find("kept while the hush lock is held") != std::string::npos && body.find("kept while the flush lock is held") != std::string::npos,
             "lines whose lock was free are written");
        That(body.find("dropped while the log lock") == std::string::npos && body.find("dropped note while") == std::string::npos && body.find("dropped rare while the log lock") == std::string::npos &&
                 body.find("dropped rare while the hush lock") == std::string::npos,
             "lines whose lock was busy at the process exit are dropped, not waited for");
        zsdk::detail::Exit(false);
    }

    std::wstring Saved(const wchar_t *name)
    {
        wchar_t buffer[1024]{};
        const DWORD size = GetEnvironmentVariableW(name, buffer, 1024);
        return size && size < 1024 ? std::wstring(buffer, size) : std::wstring();
    }

    void Paths()
    {
        const std::wstring local = Saved(L"LOCALAPPDATA");
        const std::wstring profile = Saved(L"USERPROFILE");
        const std::wstring user = Saved(L"USERNAME");
        SetEnvironmentVariableW(L"LOCALAPPDATA", L"C" L":\\Users\\Fake Person\\AppData\\Local");
        SetEnvironmentVariableW(L"USERPROFILE", L"C" L":\\Users\\Fake Person");
        SetEnvironmentVariableW(L"USERNAME", L"Fake Person");
        using zsdk::log::Masked;
        using zsdk::log::Shown;
        That(Shown(std::filesystem::path(L"C" L":\\Users\\Fake Person\\AppData\\Local\\ZoneProbe")) == "%LOCALAPPDATA%\\ZoneProbe", "a data folder is logged under %LOCALAPPDATA%");
        That(Shown(std::filesystem::path(L"c" L":\\users\\fake person\\appdata\\local\\ZoneProbe\\probe.log")) == "%LOCALAPPDATA%\\ZoneProbe\\probe.log", "the root is matched without case");
        That(Shown(std::filesystem::path(L"C" L":\\Users\\Fake Person\\Documents\\x.txt")) == "%USERPROFILE%\\Documents\\x.txt", "a profile path is logged under %USERPROFILE%");
        That(Masked("data at C" ":/Users/Fake Person/AppData/Local/ZoneProbe, ok") == "data at %LOCALAPPDATA%/ZoneProbe, ok", "forward slashes and text around the path are kept");
        That(Masked("C" ":\\Users\\Fake Person2\\x") == "C" ":\\Users\\Fake Person2\\x", "a longer folder name is not a match of the root");
        That(Masked("E" ":\\Users\\Fake Person\\Games\\Stalker2") == "E" ":\\Users\\<user>\\Games\\Stalker2", "the user name in another path is hidden");
        That(Masked("Fake Person wrote this") == "Fake Person wrote this", "the user name outside a path is left alone");
        That(Masked("D" ":\\Games\\Stalker2\\Binaries") == "D" ":\\Games\\Stalker2\\Binaries", "a path outside the profile is kept");
        That(Shown(std::filesystem::path(L"C" L":\\Users\\Fake Person\\AppData\\Local")) == "%LOCALAPPDATA%", "the root itself");
        SetEnvironmentVariableW(L"LOCALAPPDATA", local.empty() ? nullptr : local.c_str());
        SetEnvironmentVariableW(L"USERPROFILE", profile.empty() ? nullptr : profile.c_str());
        SetEnvironmentVariableW(L"USERNAME", user.empty() ? nullptr : user.c_str());
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path file = home / "log.log";
    const std::filesystem::path rolled = home / "log.1.log";
    zsdk::log::Open(file, rolled);
    That(zsdk::log::Path() == file, "the log path is the one opened");
    zsdk::Log("plain %d", 7);
    zsdk::log::SetLevel(1);
    zsdk::Note(2, "hidden note");
    zsdk::Note(1, "shown note %s", "x");
    zsdk::Rare("tag", "rare %d", 1);
    zsdk::Rare("tag", "rare %d", 2);
    zsdk::Rare("tag", "rare %d", 3);
    zsdk::Rare("other", "other %d", 1);
    zsdk::Flush();
    const std::string body = scan::Body(file);
    That(body.find("plain 7") != std::string::npos, "a log line is written");
    That(body.find("hidden note") == std::string::npos && body.find("shown note x") != std::string::npos, "notes above the level are dropped");
    That(scan::Count(body, "rare ") == 1 && body.find("rare 1") != std::string::npos, "repeated rare lines are folded");
    That(body.find("other 1") != std::string::npos, "a different tag is not folded");
    That(std::regex_search(body, std::regex("\\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] plain 7")), "the line format is HH:MM:SS.mmm [tid] text");
    zsdk::log::Hushed(GetTickCount64() + 61000);
    zsdk::Flush();
    const std::string after = scan::Body(file);
    That(after.find("tag: repeated 2 more times since the last line") != std::string::npos, "the hushed summary counts the folded lines");
    for (int i = 0; i < 4200; ++i) zsdk::Log("burst %d", i);
    That(zsdk::log::Pending() == 4096 && zsdk::log::Dropped() == 104, "the queue keeps 4096 lines and counts the dropped ones");
    zsdk::Flush();
    const std::string burst = scan::Body(file);
    That(burst.find("log dropped 104 lines") != std::string::npos, "the drop count is written once");
    That(zsdk::log::Due(GetTickCount64() + 1000) && !zsdk::log::Due(GetTickCount64() + 1000), "the flush is due every 500 ms");
    zsdk::Urgent();
    That(zsdk::log::Due(0), "an urgent flush is due at once");
    Leaving(file);
    Paths();
    zsdk::log::Roll(file, rolled);
    That(std::filesystem::exists(rolled) && !std::filesystem::exists(file), "rolling moves the log aside");
    return check::Finish("log");
}
