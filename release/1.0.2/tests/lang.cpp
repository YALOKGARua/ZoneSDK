#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

namespace
{
    using check::That;
    namespace lang = zsdk::lang;
    namespace paths = zsdk::paths;
    namespace fs = std::filesystem;

    constexpr const char *kUkrainian = "SelectedAudioLanguage = ELocalizationLanguage::English\r\nSelectedTextLanguage = ELocalizationLanguage::Ukrainian\r\nbAimAssistEnabled = true\r\n";
    constexpr const char *kEnglish = "SelectedAudioLanguage = ELocalizationLanguage::Ukrainian\r\nSelectedTextLanguage = ELocalizationLanguage::English\r\n";

    lang::Lang Windows()
    {
        return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_UKRAINIAN ? lang::Lang::Uk : lang::Lang::En;
    }

    lang::Lang Other(lang::Lang l)
    {
        return l == lang::Lang::Uk ? lang::Lang::En : lang::Lang::Uk;
    }

    std::string Acf(const char *user, const char *mounted = nullptr)
    {
        std::string out = "\"AppState\"\n{\n\t\"appid\"\t\t\"1643320\"\n\t\"name\"\t\t\"S.T.A.L.K.E.R. 2\"\n";
        if (user) out += std::string("\t\"UserConfig\"\n\t{\n\t\t\"language\"\t\t\"") + user + "\"\n\t}\n";
        if (mounted) out += std::string("\t\"MountedConfig\"\n\t{\n\t\t\"language\"\t\t\"") + mounted + "\"\n\t}\n";
        return out + "}\n";
    }

    paths::Roots Tree(const fs::path &home, std::optional<std::string_view> cfg, const std::string &acf = {})
    {
        std::error_code code;
        fs::remove_all(home, code);
        const paths::Roots r{ home / "steamapps" / "common" / "Game" / "Stalker2" / "Binaries" / "Win64" / "Stalker2-Win64-Shipping.exe", home / "local" };
        if (cfg) That(zsdk::ini::WriteAtomic(paths::AppliedSettings(r), *cfg), "the fake game settings are written in " + scan::Text(home.filename()));
        if (!acf.empty()) That(zsdk::ini::WriteAtomic(paths::SteamManifest(r), acf), "the fake Steam manifest is written in " + scan::Text(home.filename()));
        return r;
    }

    void Detection(const fs::path &home)
    {
        const lang::Lang win = Windows();
        int n = 0;
        const auto at = [&home, &n] { return home / ("case" + std::to_string(++n)); };
        That(lang::Detect(Tree(at(), kUkrainian, Acf("english"))) == lang::Lang::Uk, "the game settings Ukrainian gives Uk before Steam");
        That(lang::Detect(Tree(at(), "SelectedTextLanguage=uk\n")) == lang::Lang::Uk, "the game settings uk gives Uk");
        That(lang::Detect(Tree(at(), "\xEF\xBB\xBF  selectedtextlanguage   =  Ukrainian  \r\n")) == lang::Lang::Uk, "the key and the value match without case, spaces or a BOM");
        That(lang::Detect(Tree(at(), kEnglish, Acf("ukrainian"))) == lang::Lang::En, "the game settings English gives En before Steam");
        That(lang::Detect(Tree(at(), "SelectedTextLanguage = ELocalizationLanguage::Russian\r\n", Acf("ukrainian"))) == lang::Lang::En, "the game settings Russian gives En");
        That(lang::Detect(Tree(at(), "SelectedTextLanguage = ru\r\n", Acf("ukrainian"))) == lang::Lang::En, "the game settings ru gives En");
        That(lang::Detect(Tree(at(), "SelectedTextLanguage =\r\n", Acf("ukrainian"))) == lang::Lang::En, "an empty game language gives En");
        That(lang::Detect(Tree(at(), "SelectedTextLanguage = \x01\xFF\xFE#?\r\n", Acf("ukrainian"))) == lang::Lang::En, "a garbage game language gives En");
        That(lang::Detect(Tree(at(), "SelectedAudioLanguage = ELocalizationLanguage::Ukrainian\r\n", Acf("english"))) == lang::Lang::En, "the audio language does not count");
        That(lang::Detect(Tree(at(), std::nullopt, Acf("ukrainian"))) == lang::Lang::Uk, "a missing game settings file falls to Steam ukrainian");
        That(lang::Detect(Tree(at(), std::nullopt, Acf("english"))) == lang::Lang::En, "a missing game settings file falls to Steam english");
        const std::string garbage = std::string("\x7F") + '\0' + "garbage\xFF\xFE\r\n{}[]==\n";
        That(lang::Detect(Tree(at(), garbage, Acf("ukrainian"))) == lang::Lang::Uk, "a garbage game settings file without the key falls to Steam");
        That(lang::Detect(Tree(at(), "", Acf("ukrainian"))) == lang::Lang::Uk, "an empty game settings file falls to Steam");
        const std::string big = std::string(kUkrainian) + std::string((1u << 20), ' ');
        That(lang::Detect(Tree(at(), big, Acf("english"))) == lang::Lang::En, "a game settings file over 1 MiB is not read and falls to Steam");
        That(lang::Detect(Tree(at(), std::nullopt, Acf(nullptr, "ukrainian"))) == win, "only UserConfig counts in the Steam manifest");
        That(lang::Detect(Tree(at(), std::nullopt, "\"AppState\"\n{\n\t\"language\"\t\"ukrainian\"\n}\n")) == win, "a language outside UserConfig does not count");
        That(lang::Detect(Tree(at(), std::nullopt, "\"AppState\"\n{\n\t\"UserConfig\"\n\t{\n\t\t\"language\"\t\"ukrai")) == win, "a broken Steam manifest falls to Windows");
        That(lang::Detect(Tree(at(), std::nullopt, "}}}{{{\"")) == win, "a garbage Steam manifest falls to Windows");
        That(lang::Detect(Tree(at(), std::nullopt)) == win, "no game settings and no Steam manifest fall to the Windows UI language");
        const fs::path loose = at();
        const paths::Roots off = Tree(loose, std::nullopt, Acf("ukrainian"));
        That(lang::Detect(paths::Roots{ loose / "bin" / "Stalker2-Win64-Shipping.exe", off.local }) == win, "an exe outside the Steam tree skips the Steam step");
        That(lang::Detect(paths::Roots{}) == win, "empty roots fall to the Windows UI language");
    }

    void Cached(const fs::path &home)
    {
        if (scan::Env("ZSDK_TEST_HOME").empty())
        {
            check::Skip("the cached detection under the ZSDK_TESTS seam needs ZSDK_TEST_HOME");
            return;
        }
        const lang::Lang want = Other(Windows());
        const fs::path cfg = home / "Stalker2" / "Saved" / "GameSettings" / "AppliedSettingsWin64.cfg";
        That(paths::AppliedSettings() == cfg, "the seam points the game settings into ZSDK_TEST_HOME");
        That(zsdk::ini::WriteAtomic(cfg, want == lang::Lang::Uk ? kUkrainian : kEnglish), "the seam game settings are written");
        That(lang::Detect() == want, "the cached detection reads the game settings under the seam");
        That(zsdk::ini::WriteAtomic(cfg, want == lang::Lang::Uk ? kEnglish : kUkrainian) && lang::Detect() == want, "the detection is read once and cached");
    }
}

int main()
{
    const fs::path home = scan::EnvPath("ZSDK_TEST_HOME", fs::temp_directory_path() / "zsdk-lang");
    That(lang::Current() == lang::Lang::En, "before boot sets the language the SDK speaks English, never Ukrainian by default");
    Cached(home);
    Detection(home / "detect");
    That(static_cast<int>(lang::Lang::Count) == 2, "Ukrainian and English only");
    lang::Set("en");
    That(lang::Current() == lang::Lang::En, "en selects English");
    lang::Set("uk");
    That(lang::Current() == lang::Lang::Uk, "uk selects Ukrainian");
    const lang::Lang detected = lang::Detect();
    lang::Set(Other(detected));
    lang::Set("auto");
    That(lang::Current() == detected, "auto selects the detected game language");
    lang::Set(Other(detected));
    lang::Set("ru");
    That(lang::Current() == detected, "ru is no option, it falls back to auto");
    lang::Set(Other(detected));
    lang::Set("xx");
    That(lang::Current() == detected, "an unknown code falls back to auto");
    lang::Set(Other(detected));
    lang::Set("");
    That(lang::Current() == detected, "an empty code falls back to auto");
    lang::Set(static_cast<lang::Lang>(7));
    That(lang::Current() == lang::Lang::En, "an unknown language value becomes English");
    That(lang::Valid("auto") && lang::Valid("uk") && lang::Valid("en") && !lang::Valid("ru") && !lang::Valid("") && !lang::Valid("xx"), "valid codes");
    That(std::string(lang::Code(lang::Lang::En)) == "en" && std::string(lang::Code(lang::Lang::Uk)) == "uk" && std::string(lang::Code(lang::Lang::Count)) == "en", "codes");
    That(lang::Format(lang::Text("title", lang::Lang::Uk), { "X" }) == "Перевірка X" && lang::Format(lang::Text("title", lang::Lang::En), { "X" }) == "Probe X", "rows by language");
    That(std::string(lang::Text("no.such")) == "?" && !lang::Has("no.such") && lang::Has("say.one"), "unknown ids");
    That(lang::Format("{0}/{1}/{2}", { "a", "b" }) == "a/b/" && lang::Format("{9}", {}) == "", "format fills known slots only");
    That(lang::Ids().size() == 4, "ids are listed");
    That(lang::Slots("a {0} b {1} {0}") == std::vector<int>{ 0, 1, 0 }, "slots are listed in order");
    That(lang::Russian("\u044b") && lang::Russian("\u0401\u0436") && lang::Russian("\u044d\u0445\u043e") && lang::Russian("\u043e\u0431\u044a\u0451\u043c") && !lang::Russian("\u0490\u0430\u043d\u043e\u043a \u0457\u0436\u0430\u043a\u0430 \u0456 \u0454"), "Russian-only letters are found");
    That(lang::Russian("\u041e\u0448\u0438\u0431\u043a\u0430") && lang::Russian("\u041d\u0435\u0442") && lang::Russian("\u0412\u043a\u043b\u044e\u0447\u0435\u043d\u043e") && lang::Russian("\u0421\u043e\u0445\u0440\u0430\u043d\u0438\u0442\u044c \u0432\u0441\u0435"), "Russian-only words without Russian-only letters are found");
    That(!lang::Russian("\u0423\u0432\u0456\u043c\u043a\u043d\u0435\u043d\u043e") && !lang::Russian("\u0412\u0441\u0435 \u0433\u0430\u0440\u0430\u0437\u0434") && !lang::Russian("\u0412\u0438\u043c\u043a\u043d\u0435\u043d\u043e") && !lang::Russian("\u041d\u0435 \u0437\u0430\u0440\u0430\u0437") && !lang::Russian("\u0413\u043e\u0442\u043e\u0432\u043e"), "Ukrainian words pass");
    That(lang::Russian("x \u0447\u0442\u043e") && !lang::Russian("\u0449\u043e"), "words are matched whole");
    That(lang::Russian("\u0420\u0443\u0441\u0441\u043a\u0438\u0439") && lang::Russian("\u0440\u0443\u0441\u0441\u043a\u0430\u044f \u0432\u0435\u0440\u0441\u0438\u044f") && lang::Russian("\u043d\u0430 \u0440\u0443\u0441\u0441\u043a\u043e\u043c") &&
             lang::Russian("\u0420\u0423\u0421\u0421\u041a\u0423\u042e"),
         "a Russian language label is Russian although every letter of it is also Ukrainian");
    That(!lang::Russian("\u0420\u043e\u0441\u0456\u0439\u0441\u044c\u043a\u0430") && !lang::Russian("\u0423\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430") && !lang::Russian("\u0440\u0443\u0441\u044c\u043a\u0438\u0439 \u043b\u0456\u0442\u043e\u043f\u0438\u0441"),
         "the Ukrainian names of the languages are not Russian");
    for (const auto id : lang::Ids())
        for (const lang::Lang l : { lang::Lang::Uk, lang::Lang::En })
        {
            That(!lang::Russian(lang::Text(id, l)), std::string("no Russian letters in ") + std::string(id));
            That(zsdk::glyphs::Undrawn(lang::Text(id, l)).empty(), std::string("the paint font draws ") + std::string(id));
        }
    That(zsdk::glyphs::Drawable(0x0491) && zsdk::glyphs::Drawable(0x2116) && !zsdk::glyphs::Drawable(0x2603), "the glyph runs match the paint font");
    std::vector<std::string> many;
    for (int i = 0; i < 13; ++i) many.push_back("a" + std::to_string(i));
    That(lang::Format("{12}/{1}/{10}", many) == "a12/a1/a10" && lang::Format("{99}", many).empty(), "format fills two-digit slots");
    That(lang::Format("{123} {} {x} {1", many) == "{123} {} {x} {1", "a three-digit, empty or broken slot is plain text");
    That(lang::Slots("{12} {3} {07}") == std::vector<int>{ 12, 3, 7 } && lang::Slots("{123}{}").empty(), "slots are read with up to two digits");
    That(lang::Ids().size() == 4 && !lang::Defaults().empty(), "the add-on rows stay its own, the SDK defaults are listed apart");
    That(std::string(lang::Text("diag.title", lang::Lang::En)) == "Diagnostics" && std::string(lang::Text("diag.retry", lang::Lang::Uk)) == "Спробувати ще" &&
             lang::Has("key.press") && lang::Has("sdk.unsupported"),
         "the SDK default rows answer ids the add-on does not have, in Ukrainian and English");
    That(lang::Format(lang::Text("diag.known", lang::Lang::En), { "61bc1e03" }) == "known (61bc1e03)", "diag.known is a pattern filled with the exe build");
    That(lang::Has("wait.consoles") && std::string(lang::Text("wait.consoles", lang::Lang::En)) == "Waiting for the game consoles" &&
             std::string(lang::Text("wait.consoles", lang::Lang::Uk)) == "Чекаю на консолі гри",
         "the census-host wait of world::Grip has an SDK default row");
    for (const lang::Row &row : lang::Defaults())
        for (const lang::Lang l : { lang::Lang::Uk, lang::Lang::En })
        {
            That(!lang::Russian(row.text[static_cast<size_t>(l)]), std::string("no Russian in the SDK row ") + row.id);
            That(zsdk::glyphs::Undrawn(row.text[static_cast<size_t>(l)]).empty(), std::string("the paint font draws the SDK row ") + row.id);
        }
    return check::Finish("lang");
}
