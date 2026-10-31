#include <zsdk/present.h>

#include <windows.h>

#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace zsdk::lang
{
    namespace
    {
        constexpr size_t kMostBytes = 1u << 20;
        constexpr size_t kMostDepth = 8;

        std::atomic<Lang> current{ Lang::En };

        struct Found
        {
            Lang lang = Lang::En;
            const char *from = "Windows";
        };

        std::string Folded(std::string_view raw)
        {
            std::string out(raw);
            for (char &c : out)
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
            return out;
        }

        bool Blank(char c)
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        Lang Of(std::string_view value)
        {
            const std::string low = Folded(text::Trim(value));
            return low == "uk" || low.find("ukrainian") != std::string::npos ? Lang::Uk : Lang::En;
        }

        std::optional<std::string> GameValue(std::string_view cfg)
        {
            size_t from = cfg.size() >= 3 && cfg.substr(0, 3) == "\xEF\xBB\xBF" ? 3 : 0;
            while (from < cfg.size())
            {
                size_t end = cfg.find('\n', from);
                if (end == std::string_view::npos) end = cfg.size();
                const std::string_view line = cfg.substr(from, end - from);
                from = end + 1;
                const size_t equal = line.find('=');
                if (equal == std::string_view::npos) continue;
                if (Folded(text::Trim(line.substr(0, equal))) == "selectedtextlanguage") return text::Trim(line.substr(equal + 1));
            }
            return std::nullopt;
        }

        std::optional<std::string> SteamValue(std::string_view acf)
        {
            std::vector<std::string> path;
            std::string key;
            bool keyed = false;
            size_t at = 0;
            while (at < acf.size())
            {
                const char c = acf[at];
                if (Blank(c))
                {
                    ++at;
                    continue;
                }
                if (c == '/' && at + 1 < acf.size() && acf[at + 1] == '/')
                {
                    const size_t end = acf.find('\n', at);
                    at = end == std::string_view::npos ? acf.size() : end + 1;
                    continue;
                }
                if (c == '{' || c == '}')
                {
                    if (c == '{' && (!keyed || path.size() >= kMostDepth)) return std::nullopt;
                    if (c == '}' && path.empty()) return std::nullopt;
                    if (c == '{') path.push_back(Folded(key));
                    else path.pop_back();
                    keyed = false;
                    ++at;
                    continue;
                }
                std::string token;
                if (c == '"')
                {
                    ++at;
                    while (at < acf.size() && acf[at] != '"')
                    {
                        if (acf[at] == '\\' && at + 1 < acf.size()) ++at;
                        token += acf[at++];
                    }
                    if (at >= acf.size()) return std::nullopt;
                    ++at;
                }
                else
                    while (at < acf.size() && !Blank(acf[at]) && acf[at] != '{' && acf[at] != '}' && acf[at] != '"') token += acf[at++];
                if (!keyed)
                {
                    key = std::move(token);
                    keyed = true;
                    continue;
                }
                keyed = false;
                if (path.size() == 2 && path[0] == "appstate" && path[1] == "userconfig" && Folded(key) == "language") return token;
            }
            return std::nullopt;
        }

        bool Read(const std::filesystem::path &file, std::string &out)
        {
            out.clear();
            return !file.empty() && ini::ReadFile(file, out, kMostBytes);
        }

        Found Look(const paths::Roots &roots)
        {
            std::string body;
            if (Read(paths::AppliedSettings(roots), body))
                if (const std::optional<std::string> value = GameValue(body)) return Found{ Of(*value), "the game settings" };
            if (Read(paths::SteamManifest(roots), body))
                if (const std::optional<std::string> value = SteamValue(body)) return Found{ Of(*value), "Steam" };
            return Found{ PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_UKRAINIAN ? Lang::Uk : Lang::En, "Windows" };
        }

        const std::unordered_map<std::string_view, const Row *> &Book()
        {
            static const std::unordered_map<std::string_view, const Row *> book = [] {
                std::unordered_map<std::string_view, const Row *> made;
                for (const Row &row : app::Get().rows) made.emplace(row.id, &row);
                for (const Row &row : Defaults()) made.emplace(row.id, &row);
                return made;
            }();
            return book;
        }

        size_t Slot(Lang lang)
        {
            const auto at = static_cast<size_t>(lang);
            return at < static_cast<size_t>(Lang::Count) ? at : static_cast<size_t>(Lang::En);
        }
    }

    Lang Current()
    {
        return current.load();
    }

    void Set(Lang lang)
    {
        current = lang < Lang::Count ? lang : Lang::En;
    }

    bool Valid(std::string_view code)
    {
        return code == "auto" || code == "uk" || code == "en";
    }

    Lang Detect(const paths::Roots &roots)
    {
        return Look(roots).lang;
    }

    Lang Detect()
    {
        static const Lang detected = [] {
            const Found found = Look(paths::Current());
            Log("game language %s, from %s", found.lang == Lang::Uk ? "uk" : "en", found.from);
            return found.lang;
        }();
        return detected;
    }

    void Set(std::string_view code)
    {
        const Lang detected = Detect();
        Set(code == "uk" ? Lang::Uk : code == "en" ? Lang::En : detected);
    }

    const char *Code(Lang lang)
    {
        return lang == Lang::Uk ? "uk" : "en";
    }

    const char *Text(std::string_view id, Lang lang)
    {
        const auto &book = Book();
        const auto found = book.find(id);
        if (found == book.end()) return "?";
        return found->second->text[Slot(lang)];
    }

    const char *Text(std::string_view id)
    {
        return Text(id, Current());
    }

    bool Has(std::string_view id)
    {
        return Book().count(id) != 0;
    }

    std::vector<std::string_view> Ids()
    {
        std::vector<std::string_view> out;
        out.reserve(app::Get().rows.size());
        for (const Row &row : app::Get().rows) out.push_back(row.id);
        return out;
    }

    namespace
    {
        size_t Hole(std::string_view pattern, size_t at, int &index)
        {
            if (at >= pattern.size() || pattern[at] != '{') return 0;
            size_t end = at + 1;
            int value = 0;
            while (end < pattern.size() && end - at <= 2 && pattern[end] >= '0' && pattern[end] <= '9') value = value * 10 + (pattern[end++] - '0');
            if (end == at + 1 || end >= pattern.size() || pattern[end] != '}') return 0;
            index = value;
            return end - at + 1;
        }
    }

    std::string Format(std::string_view pattern, const std::vector<std::string> &args)
    {
        std::string out;
        out.reserve(pattern.size() + 16);
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            int index = 0;
            if (const size_t span = Hole(pattern, i, index))
            {
                if (static_cast<size_t>(index) < args.size()) out += args[static_cast<size_t>(index)];
                i += span - 1;
                continue;
            }
            out += pattern[i];
        }
        return out;
    }

    std::string Format(std::string_view pattern, std::initializer_list<std::string> args)
    {
        return Format(pattern, std::vector<std::string>(args));
    }

    std::vector<int> Slots(std::string_view pattern)
    {
        std::vector<int> out;
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            int index = 0;
            if (const size_t span = Hole(pattern, i, index))
            {
                out.push_back(index);
                i += span - 1;
            }
        }
        return out;
    }

    namespace
    {
        constexpr const char *kRussianWords[] = { "\u043d\u0435\u0442", "\u0447\u0442\u043e", "\u044d\u0442\u043e", "\u0438\u043b\u0438", "\u0435\u0441\u043b\u0438", "\u043e\u0448\u0438\u0431\u043a\u0430", "\u043e\u0448\u0438\u0431\u043a\u0438", "\u043e\u0448\u0438\u0431\u043e\u043a",
                                                  "\u0441\u043e\u0445\u0440\u0430\u043d\u0438\u0442\u044c", "\u0441\u043e\u0445\u0440\u0430\u043d\u0435\u043d\u043e", "\u0432\u043a\u043b\u044e\u0447\u0435\u043d\u043e", "\u0432\u043a\u043b\u044e\u0447\u0438\u0442\u044c",
                                                  "\u0432\u044b\u043a\u043b\u044e\u0447\u0435\u043d\u043e", "\u0432\u044b\u043a\u043b\u044e\u0447\u0438\u0442\u044c", "\u043d\u0435\u043b\u044c\u0437\u044f", "\u043c\u043e\u0436\u043d\u043e", "\u043d\u0443\u0436\u043d\u043e",
                                                  "\u0441\u0435\u0439\u0447\u0430\u0441", "\u043a\u043e\u0433\u0434\u0430", "\u0433\u0434\u0435", "\u0435\u0449\u0435", "\u0442\u043e\u043b\u044c\u043a\u043e", "\u0442\u0430\u043a\u0436\u0435", "\u0442\u043e\u0436\u0435",
                                                  "\u0442\u0435\u043f\u0435\u0440\u044c", "\u0437\u0434\u0435\u0441\u044c", "\u043d\u0430\u0436\u043c\u0438\u0442\u0435", "\u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438", "\u043f\u043e\u043a\u0430\u0437\u0430\u0442\u044c",
                                                  "\u043e\u0442\u043c\u0435\u043d\u0430", "\u0441\u0431\u0440\u043e\u0441", "\u0437\u0430\u0433\u0440\u0443\u0437\u043a\u0430", "\u043e\u0436\u0438\u0434\u0430\u043d\u0438\u0435", "\u0438\u0433\u0440\u043e\u043a",
                                                  "\u043e\u0440\u0443\u0436\u0438\u0435", "\u0433\u043e\u0442\u043e\u0432", "\u043f\u043e\u0436\u0430\u043b\u0443\u0439\u0441\u0442\u0430", "\u0441\u043f\u0430\u0441\u0438\u0431\u043e",
                                                  "\u0440\u0443\u0441\u0441\u043a\u0438\u0439", "\u0440\u0443\u0441\u0441\u043a\u0430\u044f", "\u0440\u0443\u0441\u0441\u043a\u043e\u0435", "\u0440\u0443\u0441\u0441\u043a\u043e\u0433\u043e",
                                                  "\u0440\u0443\u0441\u0441\u043a\u043e\u043c", "\u0440\u0443\u0441\u0441\u043a\u0443\u044e", "\u0440\u0443\u0441\u0441\u043a\u0438\u0445", "\u044f\u0437\u044b\u043a" };

        bool Letter(uint32_t point)
        {
            return (point >= 0x0400 && point <= 0x052F) || point == 0x02BC || point == '\'';
        }

        bool Word(const std::vector<uint32_t> &word)
        {
            if (word.empty()) return false;
            for (const char *one : kRussianWords)
                if (text::Points(one) == word) return true;
            return false;
        }
    }

    bool Russian(std::string_view utf8)
    {
        for (const uint32_t point : text::Points(utf8))
            if (point == 0x044B || point == 0x042B || point == 0x044D || point == 0x042D || point == 0x044A || point == 0x042A || point == 0x0451 || point == 0x0401)
                return true;
        std::vector<uint32_t> word;
        for (const uint32_t point : text::Points(text::Lower(utf8)))
        {
            if (Letter(point))
            {
                word.push_back(point);
                continue;
            }
            if (Word(word)) return true;
            word.clear();
        }
        return Word(word);
    }
}
