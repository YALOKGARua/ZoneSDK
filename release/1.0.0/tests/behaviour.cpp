#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/runtime.h"

#include <windows.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace
{
    using check::That;
    namespace lang = zsdk::lang;
    namespace paths = zsdk::paths;
    namespace fs = std::filesystem;

    lang::Lang Other(lang::Lang l)
    {
        return l == lang::Lang::Uk ? lang::Lang::En : lang::Lang::Uk;
    }

    paths::Roots Tree(const fs::path &home, std::optional<std::string> cfg, const std::string &acf = {})
    {
        std::error_code code;
        fs::remove_all(home, code);
        const paths::Roots r{ home / "steamapps" / "common" / "Game" / "Stalker2" / "Binaries" / "Win64" / "Stalker2-Win64-Shipping.exe", home / "local" };
        if (cfg) That(zsdk::ini::WriteAtomic(paths::AppliedSettings(r), *cfg), "the fake game settings are written in " + scan::Text(home.filename()));
        if (!acf.empty()) That(zsdk::ini::WriteAtomic(paths::SteamManifest(r), acf), "the fake Steam manifest is written in " + scan::Text(home.filename()));
        return r;
    }

    std::string Acf(const std::string &language)
    {
        return "\"AppState\"\n{\n\t\"appid\"\t\t\"1643320\"\n\t\"UserConfig\"\n\t{\n\t\t\"language\"\t\t\"" + language + "\"\n\t}\n}\n";
    }

    void GameLanguages(const fs::path &home)
    {
        int n = 0;
        const auto at = [&home, &n] { return home / ("g" + std::to_string(++n)); };
        const std::map<std::string, lang::Lang> game = {
            { "ELocalizationLanguage::Ukrainian", lang::Lang::Uk }, { "Ukrainian", lang::Lang::Uk }, { "uk", lang::Lang::Uk }, { "UK", lang::Lang::Uk },
            { "ELocalizationLanguage::UKRAINIAN", lang::Lang::Uk }, { "ELocalizationLanguage::Russian", lang::Lang::En }, { "Russian", lang::Lang::En },
            { "ru", lang::Lang::En }, { "RU", lang::Lang::En }, { "ru-RU", lang::Lang::En }, { "ELocalizationLanguage::Belarusian", lang::Lang::En },
            { "ELocalizationLanguage::English", lang::Lang::En }, { "ELocalizationLanguage::Polish", lang::Lang::En }, { "ELocalizationLanguage::German", lang::Lang::En },
        };
        for (const auto &[value, want] : game)
        {
            const paths::Roots r = Tree(at(), "SelectedTextLanguage = " + value + "\r\n", Acf(want == lang::Lang::Uk ? "english" : "ukrainian"));
            That(lang::Detect(r) == want, "the game text language " + value + " gives " + lang::Code(want) + ", whatever Steam says");
        }
        const std::map<std::string, lang::Lang> steam = {
            { "ukrainian", lang::Lang::Uk }, { "Ukrainian", lang::Lang::Uk }, { "russian", lang::Lang::En }, { "english", lang::Lang::En }, { "belarusian", lang::Lang::En }, { "polish", lang::Lang::En },
        };
        for (const auto &[value, want] : steam) That(lang::Detect(Tree(at(), std::nullopt, Acf(value))) == want, "the Steam language " + value + " gives " + lang::Code(want));
        const std::string real = "\xEF\xBB\xBF" "bAimAssistEnabled = true\r\nAudioOutputDeviceId = \xD0\x94\xD0\xB8\xD0\xBD\xD0\xB0\xD0\xBC\xD1\x96\xD0\xBA\xD0\xB8 (Realtek)\r\n"
                                 "SelectedAudioLanguage = ELocalizationLanguage::English\r\nSelectedTextLanguage = ELocalizationLanguage::Ukrainian\r\n";
        That(lang::Detect(Tree(at(), real, Acf("english"))) == lang::Lang::Uk, "a game settings file with a BOM and a Cyrillic device name still gives Uk");
        const std::string russianGame = "SelectedAudioLanguage = ELocalizationLanguage::Ukrainian\r\nSelectedTextLanguage = ELocalizationLanguage::Russian\r\n";
        That(lang::Detect(Tree(at(), russianGame, Acf("ukrainian"))) == lang::Lang::En, "a Russian game text language gives En even with Ukrainian audio and a Ukrainian Steam client");
    }

    void Codes()
    {
        const lang::Lang detected = lang::Detect();
        for (const char *code : { "ru", "RU", "Ru", "ru-RU", "ru_RU", "rus", "russian", "Russian", "be", "by", "auto", "", " uk", "uk ", "UK", "EN", "ukrainian", "uk-UA" })
        {
            lang::Set(Other(detected));
            lang::Set(code);
            That(lang::Current() == detected, std::string("the code '") + code + "' falls back to the detected game language");
        }
        lang::Set(lang::Lang::En);
        lang::Set("uk");
        That(lang::Current() == lang::Lang::Uk, "uk selects Ukrainian");
        lang::Set("en");
        That(lang::Current() == lang::Lang::En, "en selects English");
        for (const char *code : { "ru", "RU", "Ru", "ru-RU", "russian", "rus", " uk", "uk ", "UK", "Auto", "AUTO", "EN" }) That(!lang::Valid(code), std::string("the code '") + code + "' is not a valid stored language");
        bool onlyTwo = true;
        for (int i = 0; i < 256; ++i)
        {
            const std::string code = lang::Code(static_cast<lang::Lang>(i));
            onlyTwo = onlyTwo && (code == "uk" || code == "en");
        }
        That(onlyTwo, "every language value names uk or en, never a third code");
        lang::Set(static_cast<lang::Lang>(200));
        That(lang::Current() == lang::Lang::En, "an out-of-range language value is English, not Ukrainian");
    }

    void Stored(const fs::path &folder)
    {
        const std::map<std::string, std::string> stored = {
            { "ru", "auto" }, { "RU", "auto" }, { "Russian", "auto" }, { "ru-RU", "auto" }, { "", "auto" }, { "xx", "auto" }, { "UK", "uk" }, { "En", "en" }, { "AUTO", "auto" },
        };
        for (const auto &[value, want] : stored)
        {
            That(zsdk::ini::WriteAtomic(folder / "probe.ini", "[general]\r\nlanguage=" + value + "\r\n") && zsdk::ini::Open(folder), "the data folder opens with language=" + value);
            That(zsdk::ini::Language() == want, "a stored language '" + value + "' is used as " + want);
            That(scan::Body(folder / "probe.ini").find("language=" + want + "\r\n") != std::string::npos, "the rewritten ini carries language=" + want + " for '" + value + "'");
        }
        That(zsdk::ini::WriteAtomic(folder / "probe.ini", "[general]\r\nenabled=1\r\n") && zsdk::ini::Open(folder) && zsdk::ini::Language() == "auto", "an ini without a language line gives auto");
        std::error_code code;
        fs::remove(folder / "probe.ini", code);
        That(zsdk::ini::Open(folder) && zsdk::ini::Language() == "auto", "a fresh ini gives auto");
    }

    bool Cyrillic(std::string_view utf8)
    {
        for (const uint32_t point : zsdk::text::Points(utf8))
            if (point >= 0x0400 && point <= 0x052F) return true;
        return false;
    }

    void Defaults()
    {
        for (const lang::Row &row : lang::Defaults())
        {
            const std::string id = row.id ? row.id : "";
            const char *uk = row.text[static_cast<size_t>(lang::Lang::Uk)];
            const char *en = row.text[static_cast<size_t>(lang::Lang::En)];
            That(id.find(".ru") == std::string::npos && id.find("russian") == std::string::npos, "no SDK row offers a Russian language: " + id);
            That(uk && Cyrillic(uk), "the Ukrainian slot of the SDK row " + id + " is Ukrainian text");
            That(en && !Cyrillic(en), "the English slot of the SDK row " + id + " has no Cyrillic letters");
        }
    }

    void Put(const wchar_t *name, const std::wstring &value)
    {
        SetEnvironmentVariableW(name, value.empty() ? nullptr : value.c_str());
    }

    std::wstring Get(const wchar_t *name)
    {
        wchar_t buffer[1024]{};
        const DWORD size = GetEnvironmentVariableW(name, buffer, 1024);
        return size && size < 1024 ? std::wstring(buffer, size) : std::wstring();
    }

    void Masks()
    {
        const std::wstring local = Get(L"LOCALAPPDATA");
        const std::wstring profile = Get(L"USERPROFILE");
        const std::wstring user = Get(L"USERNAME");
        const std::string c = std::string(1, 'C') + ":";
        const std::string d = std::string(1, 'D') + ":";
        const std::string e = std::string(1, 'E') + ":";
        const std::wstring home = std::wstring(1, L'C') + L":\\Users\\Jane Roe";
        Put(L"USERPROFILE", home);
        Put(L"LOCALAPPDATA", home + L"\\AppData\\Local");
        Put(L"USERNAME", L"Jane Roe");
        const auto is = [](const std::string &in, const std::string &want) {
            const std::string got = zsdk::log::Masked(in);
            That(got == want, "masked '" + want + "', got '" + got + "'");
        };
        is("data at " + c + "\\Users\\Jane Roe\\AppData\\Local\\ZoneX", "data at %LOCALAPPDATA%\\ZoneX");
        is(c + "/Users/Jane Roe/AppData/Local/ZoneX/x.log", "%LOCALAPPDATA%/ZoneX/x.log");
        is(std::string(1, 'c') + ":\\users\\jane roe\\appdata\\local\\zonex", "%LOCALAPPDATA%\\zonex");
        is(c + "\\Users\\Jane Roe\\Documents\\a.txt", "%USERPROFILE%\\Documents\\a.txt");
        is(c + "\\Users\\Jane Roe\\AppData\\LocalLow\\a", "%USERPROFILE%\\AppData\\LocalLow\\a");
        is(c + "\\Users\\Jane Roe\\AppData\\Local-old\\a", "%USERPROFILE%\\AppData\\Local-old\\a");
        is("\"" + c + "\\Users\\Jane Roe\"", "\"%USERPROFILE%\"");
        is(c + "\\Users\\Jane Roebuck\\x", c + "\\Users\\Jane Roebuck\\x");
        is(d + "\\Games\\Jane Roe\\mods", d + "\\Games\\<user>\\mods");
        is(e + "\\Jane Roe", e + "\\<user>");
        is("plain Jane Roe text", "plain Jane Roe text");
        Put(L"LOCALAPPDATA", home + L"\\AppData\\Local\\");
        is(c + "\\Users\\Jane Roe\\AppData\\Local\\ZoneX", "%LOCALAPPDATA%\\ZoneX");
        Put(L"LOCALAPPDATA", std::wstring(1, L'D') + L":\\Local");
        is(d + "\\Local\\ZoneX and " + c + "\\Users\\Jane Roe\\x", "%LOCALAPPDATA%\\ZoneX and %USERPROFILE%\\x");
        const std::wstring olena = std::wstring(1, L'C') + L":\\Users\\\u041e\u043b\u0435\u043d\u0430";
        Put(L"USERPROFILE", olena);
        Put(L"LOCALAPPDATA", olena + L"\\AppData\\Local");
        Put(L"USERNAME", L"\u041e\u043b\u0435\u043d\u0430");
        That(zsdk::log::Shown(fs::path(olena + L"\\AppData\\Local\\ZoneX\\x.log")) == "%LOCALAPPDATA%\\ZoneX\\x.log", "a Cyrillic profile is masked in a shown path");
        is(d + "\\\xD0\x9E\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB0\\mods", d + "\\<user>\\mods");
        Put(L"USERNAME", L"");
        Put(L"USERPROFILE", L"");
        Put(L"LOCALAPPDATA", L"");
        is(c + "\\Users\\Jane Roe\\x", c + "\\Users\\Jane Roe\\x");
        Put(L"LOCALAPPDATA", local);
        Put(L"USERPROFILE", profile);
        Put(L"USERNAME", user);
    }

    void Report(const fs::path &folder)
    {
        const std::wstring local = Get(L"LOCALAPPDATA");
        const std::wstring profile = Get(L"USERPROFILE");
        const std::wstring user = Get(L"USERNAME");
        That(zsdk::ini::Open(folder), "the data folder opens for the report");
        const std::string c = std::string(1, 'C') + ":";
        zsdk::Log("data at %s\\Users\\Jane Roe\\AppData\\Local\\ZoneSdkProbe, written by an older ZoneSDK", c.c_str());
        zsdk::Log("mods in %s\\Jane Roe\\mods", (std::string(1, 'E') + ":").c_str());
        zsdk::Flush();
        const std::wstring home = std::wstring(1, L'C') + L":\\Users\\Jane Roe";
        Put(L"USERPROFILE", home);
        Put(L"LOCALAPPDATA", home + L"\\AppData\\Local");
        Put(L"USERNAME", L"Jane Roe");
        const std::string text = zsdk::report::Text(nullptr);
        That(text.find("data at %LOCALAPPDATA%\\ZoneSdkProbe, written by an older ZoneSDK") != std::string::npos, "an unmasked older log line is masked in the report");
        That(text.find("mods in " + std::string(1, 'E') + ":\\<user>\\mods") != std::string::npos, "a user name inside a path of the log tail is hidden in the report");
        That(text.find("Jane Roe") == std::string::npos, "the report never names the user");
        Put(L"LOCALAPPDATA", local);
        Put(L"USERPROFILE", profile);
        Put(L"USERNAME", user);
    }

    struct Frame
    {
        uint64_t at;
        bool cursor;
        bool stirred;
    };

    std::vector<bool> Run(std::initializer_list<Frame> frames)
    {
        zsdk::detail::input::Glance g;
        std::vector<bool> out;
        for (const Frame &f : frames) out.push_back(zsdk::detail::input::Steady(g, f.cursor, f.stirred, f.at));
        return out;
    }

    void Cursor()
    {
        That(Run({ { 1000, true, true }, { 1100, true, false }, { 1399, true, false }, { 1400, true, false } }) == std::vector<bool>{ false, false, false, true },
             "closing the ReShade overlay over a visible cursor delays the menu by 400 ms from the last stirred frame");
        That(Run({ { 1000, true, false }, { 1500, true, false }, { 5000, true, true }, { 5300, true, false }, { 5399, true, false }, { 5400, true, false } }) ==
                 std::vector<bool>{ false, true, false, false, false, true },
             "an Alt tap in a long inventory session drops the menu for 400 ms only");
        std::initializer_list<Frame> blink = { { 1000, true, false }, { 1016, false, false }, { 1033, true, false }, { 1050, false, false }, { 1066, true, false }, { 1083, false, false },
                                               { 1100, true, false }, { 1116, false, false }, { 1133, true, false }, { 1150, false, false }, { 1500, true, false }, { 1516, false, false } };
        bool any = false;
        for (const bool menu : Run(blink)) any = any || menu;
        That(!any, "a cursor that blinks every other frame is never a menu");
        That(Run({ { 1000, true, false }, { 2000, true, false } }) == std::vector<bool>{ false, true }, "sparse frames count from the first visible frame");
        That(Run({ { 1000, false, false }, { 1001, true, false }, { 1401, true, false } }) == std::vector<bool>{ false, false, true }, "a cursor shown after a hidden frame counts from its own first frame");
        const uint64_t big = 0xFFFFFFFFFFFF0000ull;
        That(Run({ { big, true, false }, { big + 400, true, false } }) == std::vector<bool>{ false, true }, "large tick values keep the 400 ms rule");
    }

    void Registry()
    {
        const std::map<std::string, std::string> released = {
            { "ride", "ZoneRide" }, { "heli", "ZoneHeli" }, { "balloon", "ZoneBalloon" }, { "mount", "ZoneMount" }, { "skif", "ZoneSkif" }, { "zipline", "ZoneZipline" },
            { "grenades", "ZoneGrenades" }, { "underground", "ZoneUnderground" }, { "limansk", "ZoneLimansk" }, { "mercy", "Mercy" }, { "spawner", "Spawner" }, { "pulse", "ZonePulse" },
            { "coop", "ZoneCoop" }, { "newland", "ZoneNewLand" }, { "cheats", "ZoneCheats" }, { "blinddog", "ZoneBlindDog" }, { "wounds", "ZoneWounds" }, { "template", "ZoneTemplate" },
        };
        std::map<std::string, std::string> listed;
        for (const zsdk::registry::Row &row : zsdk::registry::Rows()) listed[row.id] = row.tag;
        That(listed == released, "every registry row carries the tag its released add-on takes in Local\\<tag>.Addon.<pid>, " + std::to_string(listed.size()) + " rows");
        for (const zsdk::registry::Row &row : zsdk::registry::Rows())
        {
            std::string both = std::string(row.id) + " " + row.tag;
            for (char &ch : both)
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + 32);
            bool clean = true;
            for (const char *never : { "inspect", "look", "faction", "terra", "imgui", "livemap", "puppet", "giants", "probe" }) clean = clean && both.find(never) == std::string::npos;
            That(clean, std::string("the registry row ") + row.id + " names no private or unreleased add-on");
        }
    }
}

int main()
{
    const fs::path home = scan::EnvPath("ZSDK_TEST_HOME", fs::temp_directory_path() / "zsdk-behaviour");
    GameLanguages(home / "lang");
    Codes();
    const fs::path folder = home / "data";
    std::error_code code;
    fs::remove_all(folder, code);
    Stored(folder);
    Defaults();
    Masks();
    Report(home / "report");
    Cursor();
    Registry();
    return check::Finish("behaviour");
}
