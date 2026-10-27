#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/ledger_raw.h>
#include <zsdk/test/scan.h>

#include "../src/detail/runtime.h"
#include "zsdk_build.h"

#include <windows.h>

#include <regex>
#include <string>

namespace
{
    using check::That;

    std::string Section()
    {
        return "probe section: 3 widgets\r\n";
    }

    std::string Variable(const wchar_t *name)
    {
        wchar_t buffer[1024]{};
        const DWORD size = GetEnvironmentVariableW(name, buffer, 1024);
        std::string out;
        for (DWORD i = 0; i < size && i < 1024; ++i) out.push_back(buffer[i] < 0x80 ? static_cast<char>(buffer[i]) : '?');
        return out;
    }
}

int main()
{
    namespace report = zsdk::report;
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path folder = home / "report";
    const std::filesystem::path games = home / "game-bin";
    std::error_code code;
    std::filesystem::remove_all(folder, code);
    std::filesystem::remove_all(games, code);
    std::filesystem::create_directories(games, code);
    That(zsdk::ini::WriteAtomic(games / "ZoneOne.addon64", "abc") && zsdk::ini::WriteAtomic(games / "ZoneTwo.addon64", "abcdef") && zsdk::ini::WriteAtomic(games / "notes.txt", "x"), "a fake game folder with two add-ons");
    zsdk::detail::ReportFolder(games.c_str());
    That(report::File().empty() && !report::Save("x"), "without a data folder the report has no file and is not saved");
    That(zsdk::ini::Open(folder), "the data folder opens");
    That(report::File() == folder / "report.txt", "the report file lives in the data folder");
    That(zsdk::ledger::raw::Disable("Probe.Off"), "a key is disabled");
    for (int i = 0; i < 200; ++i) zsdk::Log("tail line %d", i);
    const std::string profile = Variable(L"USERPROFILE");
    zsdk::Log("planted %s\\Secret\\file.txt", profile.c_str());
    zsdk::Hooks h;
    h.report = &Section;
    zsdk::hooks::Bind(h);
    const std::string text = report::Text(nullptr);
    That(text.rfind("ZoneSDK report, ", 0) == 0, "the report starts with its head line");
    That(text.find("add-on: ZoneSdkProbe 0.0.1, tag ZoneSdkProbe, build " + zsdk::ledger::Addon()) != std::string::npos, "the add-on name, version, tag and build are in the report");
    That(text.find(std::string("zsdk: ") + ZSDK_VERSION + " manifest " + ZSDK_MANIFEST) != std::string::npos, "the SDK version and manifest are in the report");
    That(text.find("game exe: zsdk-report.exe, not the Steam build") != std::string::npos, "the game exe line names the exe file only");
    That(text.find("reshade: unknown") != std::string::npos, "an unknown ReShade module is said so");
    That(text.find("add-ons next to the game exe: 2\r\n  ZoneOne.addon64, 3 bytes\r\n  ZoneTwo.addon64, 6 bytes\r\n") != std::string::npos && text.find("notes.txt") == std::string::npos,
         "the *.addon64 files next to the exe are listed with their sizes, nothing else");
    That(text.find("[safety]\r\nsession " + zsdk::ledger::Current()) != std::string::npos && text.find("disabled Probe.Off") != std::string::npos, "the [safety] lists are in the report");
    That(text.find("[ZoneSdkProbe]\r\nprobe section: 3 widgets\r\n") != std::string::npos, "the add-on appends its own section through the report hook");
    That(text.find("tail line 199") != std::string::npos && text.find("tail line 121\r\n") != std::string::npos && text.find("tail line 120\r\n") == std::string::npos,
         "the report carries the last 80 log lines");
    That(text.find("[log] last 80 lines of ") != std::string::npos, "the log line count and file are named");
    That(text.find("planted %USERPROFILE%\\Secret\\file.txt") != std::string::npos, "a profile path inside a log line is masked");
    const std::string user = Variable(L"USERNAME");
    That(!profile.empty() && text.find(profile) == std::string::npos, "the report has no full profile path");
    That(user.size() < 2 || !std::regex_search(text, std::regex("[\\\\/]" + std::regex_replace(user, std::regex("[.^$|()\\[\\]{}*+?\\\\]"), "\\$&") + "([\\\\/\"' ]|$)", std::regex::icase)),
         "the report has no user name inside a path");
    That(report::Save(text) && scan::Body(report::File()) == text, "the report is saved in the data folder as built");
    zsdk::Flush();
    That(scan::Body(zsdk::ini::LogPath()).find("report saved to ") != std::string::npos, "saving is logged with the masked file path");
    const std::string version = report::Version(GetModuleHandleW(L"kernel32.dll"));
    That(std::regex_match(version, std::regex("\\d+\\.\\d+\\.\\d+\\.\\d+")), "the version resource of a module is read in place: kernel32 " + version);
    That(report::Version(nullptr) == "unknown" && report::Version(GetModuleHandleW(nullptr)) == "unknown", "a module without a version resource is unknown");
    zsdk::Hooks none;
    zsdk::hooks::Bind(none);
    That(report::Text(nullptr).find("[ZoneSdkProbe]") == std::string::npos, "without a report hook there is no add-on section");
    zsdk::detail::ReportFolder(nullptr);
    return check::Finish("report");
}
