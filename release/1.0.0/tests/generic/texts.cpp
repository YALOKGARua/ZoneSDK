#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <algorithm>
#include <set>
#include <string>

namespace
{
    bool Ends(const std::string &text, const std::string &tail)
    {
        return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
    }

    bool RussianCode(const std::string &literal)
    {
        std::string low = literal;
        for (char &c : low)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        for (const char *code : { "ru", "rus", "russian", "ru-ru", "ru_ru" })
            if (low == code) return true;
        return Ends(low, ".ru") || Ends(low, ".rus") || Ends(low, ".russian");
    }
}

int main()
{
    using check::That;
    namespace lang = zsdk::lang;
    const addon::Tree t = addon::Load();
    That(static_cast<int>(lang::Lang::Count) == 2, "the SDK knows Ukrainian and English only");
    const auto defaults = lang::Defaults();
    That(!defaults.empty(), "the SDK ships default rows, " + std::to_string(defaults.size()));
    for (const lang::Row &row : defaults)
    {
        const std::string id = row.id ? row.id : "";
        const std::string uk = row.text[static_cast<size_t>(lang::Lang::Uk)] ? row.text[static_cast<size_t>(lang::Lang::Uk)] : "";
        const std::string en = row.text[static_cast<size_t>(lang::Lang::En)] ? row.text[static_cast<size_t>(lang::Lang::En)] : "";
        That(!id.empty() && !uk.empty() && !en.empty() && lang::Has(id), "the SDK default row has both languages: " + id);
        That(!lang::Russian(uk) && !lang::Russian(en), "no Russian in the SDK default row " + id);
        std::vector<int> a = lang::Slots(uk);
        std::vector<int> b = lang::Slots(en);
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        That(a == b, "uk and en use the same {n} slots in the SDK default row " + id);
        That(zsdk::glyphs::Undrawn(uk).empty() && zsdk::glyphs::Undrawn(en).empty(), "the paint font draws the SDK default row " + id);
    }
    const auto ids = lang::Ids();
    That(!ids.empty(), "the add-on has language rows, " + std::to_string(ids.size()));
    std::set<std::string> seen;
    for (const auto id : ids)
    {
        That(seen.insert(std::string(id)).second, "row ids are unique: " + std::string(id));
        const std::string uk = lang::Text(id, lang::Lang::Uk);
        const std::string en = lang::Text(id, lang::Lang::En);
        That(!uk.empty() && uk != "?" && !en.empty() && en != "?", "both languages have text: " + std::string(id));
        That(!lang::Russian(uk) && !lang::Russian(en), "no Russian letters: " + std::string(id));
        std::vector<int> a = lang::Slots(uk);
        std::vector<int> b = lang::Slots(en);
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        a.erase(std::unique(a.begin(), a.end()), a.end());
        b.erase(std::unique(b.begin(), b.end()), b.end());
        That(a == b, "uk and en use the same {n} slots: " + std::string(id));
        That(zsdk::glyphs::Undrawn(uk).empty() && zsdk::glyphs::Undrawn(en).empty(), "the paint font draws every sign of " + std::string(id) + zsdk::glyphs::Undrawn(uk) + zsdk::glyphs::Undrawn(en));
    }
    lang::Set(lang::Detect() == lang::Lang::Uk ? lang::Lang::En : lang::Lang::Uk);
    lang::Set("ru");
    That(!lang::Valid("ru") && lang::Current() == lang::Detect(), "ru never selects a Russian option and equals Detect()");
    std::vector<std::filesystem::path> files = t.present;
    files.insert(files.end(), t.overlay.begin(), t.overlay.end());
    files.push_back(t.entry);
    for (const auto &file : files)
    {
        const scan::Lexed lexed = scan::Lex(scan::Body(file));
        bool offered = false;
        std::string named;
        for (const std::string &literal : lexed.literals)
            if (RussianCode(literal))
            {
                offered = true;
                named = literal;
            }
        That(!offered, "no Russian language is offered in " + addon::Rel(t, file) + (named.empty() ? std::string() : ": \"" + named + "\""));
    }
    if (!zsdk::app::Get().schema) check::Skip("zsdk-texts: the add-on has no settings schema, its language is the game language");
    else
    {
        const std::filesystem::path fresh = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path()) / "zsdk-texts-fresh";
        std::error_code code;
        std::filesystem::remove_all(fresh, code);
        std::filesystem::create_directories(fresh, code);
        That(zsdk::ini::Open(fresh), "a fresh data folder opens");
        const std::string chosen = zsdk::ini::Language();
        That(chosen == "auto", "a fresh ini gives the language auto (the game language: uk gives UA, every other culture EN), the add-on default is " + chosen);
        zsdk::ini::Close();
    }
    return check::Finish("zsdk-texts");
}
