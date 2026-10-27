#pragma once

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace scan
{
    struct Lexed
    {
        std::vector<std::string> literals;
        std::set<std::string> words;
        std::string code;
        int comments = 0;
        size_t firstComment = 0;
        bool closed = true;
    };

    struct Function
    {
        std::string name;
        size_t at = 0;
        std::string body;
    };

    inline std::string Body(const std::filesystem::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    inline bool Ident(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    inline size_t LineOf(const std::string &body, size_t at)
    {
        size_t line = 1;
        for (size_t i = 0; i < at && i < body.size(); ++i)
            if (body[i] == '\n') ++line;
        return line;
    }

    inline Lexed Lex(const std::string &body)
    {
        Lexed out;
        out.code.reserve(body.size());
        size_t i = 0;
        while (i < body.size())
        {
            const char c = body[i];
            if (c == '/' && i + 1 < body.size() && (body[i + 1] == '/' || body[i + 1] == '*'))
            {
                if (out.comments++ == 0) out.firstComment = LineOf(body, i);
                if (body[i + 1] == '/')
                {
                    while (i < body.size() && body[i] != '\n') ++i;
                }
                else
                {
                    const size_t end = body.find("*/", i + 2);
                    i = end == std::string::npos ? body.size() : end + 2;
                }
                out.code += ' ';
                continue;
            }
            if (c == '"')
            {
                const bool raw = i > 0 && body[i - 1] == 'R' && (i < 2 || !Ident(body[i - 2]) || body[i - 2] == 'L' || body[i - 2] == 'u' || body[i - 2] == 'U' || body[i - 2] == '8');
                if (raw)
                {
                    const size_t open = body.find('(', i + 1);
                    if (open == std::string::npos)
                    {
                        out.closed = false;
                        break;
                    }
                    const std::string tail = ")" + body.substr(i + 1, open - i - 1) + "\"";
                    const size_t end = body.find(tail, open + 1);
                    if (end == std::string::npos)
                    {
                        out.closed = false;
                        break;
                    }
                    out.literals.push_back(body.substr(open + 1, end - open - 1));
                    i = end + tail.size();
                    out.code += "\"\"";
                    continue;
                }
                std::string text;
                ++i;
                while (i < body.size() && body[i] != '"' && body[i] != '\n')
                {
                    if (body[i] == '\\' && i + 1 < body.size())
                    {
                        text += body[i];
                        ++i;
                    }
                    text += body[i];
                    ++i;
                }
                if (i >= body.size() || body[i] != '"') out.closed = false;
                ++i;
                out.literals.push_back(text);
                out.code += "\"\"";
                continue;
            }
            if (c == '\'')
            {
                const bool digit = i > 0 && std::isxdigit(static_cast<unsigned char>(body[i - 1])) && i + 1 < body.size() && std::isxdigit(static_cast<unsigned char>(body[i + 1]));
                if (digit)
                {
                    ++i;
                    continue;
                }
                ++i;
                while (i < body.size() && body[i] != '\'' && body[i] != '\n')
                {
                    if (body[i] == '\\') ++i;
                    ++i;
                }
                if (i >= body.size() || body[i] != '\'') out.closed = false;
                ++i;
                out.code += "' '";
                continue;
            }
            if (Ident(c))
            {
                const size_t from = i;
                while (i < body.size() && Ident(body[i])) ++i;
                const std::string word = body.substr(from, i - from);
                out.words.insert(word);
                out.code += word;
                continue;
            }
            out.code += c;
            ++i;
        }
        return out;
    }

    inline std::string Text(const std::filesystem::path &path)
    {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
    }

    inline std::string Generic(const std::filesystem::path &path)
    {
        const std::u8string text = path.generic_u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
    }

    inline std::vector<std::filesystem::path> Files(const std::filesystem::path &dir, const std::set<std::string> &kinds, bool deep)
    {
        std::vector<std::filesystem::path> out;
        std::error_code code;
        if (!std::filesystem::is_directory(dir, code)) return out;
        const auto take = [&](const std::filesystem::directory_entry &entry) {
            std::error_code kind;
            if (entry.is_regular_file(kind) && kinds.count(Text(entry.path().extension()))) out.push_back(entry.path());
        };
        if (deep)
        {
            std::filesystem::recursive_directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, code);
            for (; !code && it != std::filesystem::recursive_directory_iterator(); it.increment(code)) take(*it);
        }
        else
        {
            std::filesystem::directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, code);
            for (; !code && it != std::filesystem::directory_iterator(); it.increment(code)) take(*it);
        }
        return out;
    }

    inline std::vector<std::filesystem::path> Dirs(const std::filesystem::path &dir)
    {
        std::vector<std::filesystem::path> out;
        std::error_code code;
        if (!std::filesystem::is_directory(dir, code)) return out;
        std::filesystem::directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, code);
        for (; !code && it != std::filesystem::directory_iterator(); it.increment(code))
        {
            std::error_code kind;
            if (it->is_directory(kind) && !it->is_symlink(kind)) out.push_back(it->path());
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    inline std::wstring Wide(const char *name)
    {
        std::wstring key;
        for (const char *walk = name; walk && *walk; ++walk) key.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
        std::wstring value(128, L'\0');
        for (;;)
        {
            const DWORD got = GetEnvironmentVariableW(key.c_str(), value.data(), static_cast<DWORD>(value.size()));
            if (got == 0) return {};
            if (got < value.size())
            {
                value.resize(got);
                return value;
            }
            value.resize(got);
        }
    }

    inline std::string Env(const char *name, const std::string &fallback = {})
    {
        const std::wstring value = Wide(name);
        if (value.empty()) return fallback;
        const int size = WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string out(size > 0 ? static_cast<size_t>(size) : 0, '\0');
        if (size > 0) WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    inline std::filesystem::path EnvPath(const char *name, const std::filesystem::path &fallback = {})
    {
        const std::wstring value = Wide(name);
        return value.empty() ? fallback : std::filesystem::path(value);
    }

    inline std::filesystem::path AddonRoot()
    {
        return EnvPath("ZSDK_ADDON_SOURCE");
    }

    inline std::filesystem::path SdkRoot()
    {
        return EnvPath("ZSDK_HOME");
    }

    inline std::string Trim(const std::string &raw)
    {
        size_t from = 0, to = raw.size();
        while (from < to && std::isspace(static_cast<unsigned char>(raw[from]))) ++from;
        while (to > from && std::isspace(static_cast<unsigned char>(raw[to - 1]))) --to;
        return raw.substr(from, to - from);
    }

    inline std::vector<std::string> List(const std::string &csv)
    {
        std::vector<std::string> out;
        size_t from = 0;
        while (from <= csv.size())
        {
            size_t comma = csv.find(',', from);
            if (comma == std::string::npos) comma = csv.size();
            const std::string one = Trim(csv.substr(from, comma - from));
            if (!one.empty()) out.push_back(one);
            from = comma + 1;
        }
        return out;
    }

    inline std::map<std::string, std::string> Config(const std::filesystem::path &file)
    {
        std::map<std::string, std::string> out;
        std::istringstream lines(Body(file));
        std::string line;
        while (std::getline(lines, line))
        {
            const size_t equal = line.find('=');
            if (equal == std::string::npos) continue;
            out[Trim(line.substr(0, equal))] = Trim(line.substr(equal + 1));
        }
        return out;
    }

    inline size_t Close(const std::string &code, size_t open)
    {
        int depth = 0;
        for (size_t i = open; i < code.size(); ++i)
        {
            if (code[i] == '{') ++depth;
            else if (code[i] == '}' && --depth == 0) return i;
        }
        return std::string::npos;
    }

    inline std::string Before(const std::string &code, size_t at)
    {
        size_t i = at;
        while (i > 0 && std::isspace(static_cast<unsigned char>(code[i - 1]))) --i;
        size_t from = i;
        while (from > 0 && Ident(code[from - 1])) --from;
        return code.substr(from, i - from);
    }

    inline std::vector<Function> Functions(const std::string &code)
    {
        static const std::set<std::string> kKeywords = { "if", "for", "while", "switch", "catch", "return", "sizeof", "alignof", "decltype", "noexcept", "requires", "static_assert", "defined", "__except", "__try" };
        std::vector<Function> out;
        size_t i = 0;
        while (i < code.size())
        {
            const size_t open = code.find('{', i);
            if (open == std::string::npos) break;
            size_t back = open;
            while (back > 0 && std::isspace(static_cast<unsigned char>(code[back - 1]))) --back;
            std::string tail;
            size_t probe = back;
            for (int hops = 0; hops < 4; ++hops)
            {
                const std::string word = Before(code, probe);
                if (word == "const" || word == "noexcept" || word == "override" || word == "final" || word == "mutable")
                {
                    probe -= word.size();
                    while (probe > 0 && std::isspace(static_cast<unsigned char>(code[probe - 1]))) --probe;
                    continue;
                }
                break;
            }
            if (probe > 0 && code[probe - 1] == ')')
            {
                int depth = 0;
                size_t at = probe - 1;
                for (;; --at)
                {
                    if (code[at] == ')') ++depth;
                    else if (code[at] == '(' && --depth == 0) break;
                    if (at == 0) break;
                }
                const std::string name = Before(code, at);
                size_t nameAt = at;
                while (nameAt > 0 && std::isspace(static_cast<unsigned char>(code[nameAt - 1]))) --nameAt;
                const bool lambda = nameAt > 0 && code[nameAt - 1] == ']';
                if (!name.empty() && !kKeywords.count(name) && !lambda && !std::isdigit(static_cast<unsigned char>(name[0])))
                {
                    const size_t end = Close(code, open);
                    if (end == std::string::npos) break;
                    out.push_back(Function{ name, open, code.substr(open, end - open + 1) });
                    i = end + 1;
                    continue;
                }
            }
            i = open + 1;
        }
        return out;
    }

    inline std::set<std::string> Calls(const std::string &code)
    {
        std::set<std::string> out;
        for (size_t i = 0; i < code.size(); ++i)
        {
            if (!Ident(code[i]) || (i > 0 && Ident(code[i - 1]))) continue;
            size_t end = i;
            while (end < code.size() && Ident(code[end])) ++end;
            size_t next = end;
            while (next < code.size() && std::isspace(static_cast<unsigned char>(code[next]))) ++next;
            if (next < code.size() && code[next] == '(') out.insert(code.substr(i, end - i));
            i = end;
        }
        return out;
    }

    inline size_t Count(const std::string &text, const std::string &what)
    {
        size_t n = 0;
        for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) ++n;
        return n;
    }

    inline std::vector<std::string> Tokens(const std::string &code)
    {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < code.size())
        {
            const char c = code[i];
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                ++i;
                continue;
            }
            if (Ident(c))
            {
                const size_t from = i;
                while (i < code.size() && Ident(code[i])) ++i;
                out.push_back(code.substr(from, i - from));
                continue;
            }
            if (c == ':' && i + 1 < code.size() && code[i + 1] == ':')
            {
                out.push_back("::");
                i += 2;
                continue;
            }
            out.push_back(std::string(1, c));
            ++i;
        }
        return out;
    }

    struct Piece
    {
        std::string text;
        bool prefix = false;
    };

    inline std::vector<Piece> Rule(const std::string &rule)
    {
        std::vector<Piece> out;
        for (const std::string &token : Tokens(rule))
        {
            if (token == "*" && !out.empty() && Ident(out.back().text[0])) out.back().prefix = true;
            else out.push_back(Piece{ token, false });
        }
        return out;
    }

    inline size_t Seek(const std::vector<std::string> &tokens, const std::string &rule)
    {
        const std::vector<Piece> want = Rule(rule);
        if (want.empty() || tokens.size() < want.size()) return std::string::npos;
        for (size_t i = 0; i + want.size() <= tokens.size(); ++i)
        {
            bool all = true;
            for (size_t k = 0; k < want.size() && all; ++k)
            {
                const std::string &have = tokens[i + k];
                all = want[k].prefix ? have.compare(0, want[k].text.size(), want[k].text) == 0 : have == want[k].text;
            }
            if (all) return i;
        }
        return std::string::npos;
    }

    inline bool Uses(const std::vector<std::string> &tokens, const std::string &rule)
    {
        return Seek(tokens, rule) != std::string::npos;
    }

    struct Tick
    {
        size_t line = 0;
        std::string text;
    };

    inline std::vector<Tick> Ticks(const std::string &code)
    {
        static const std::regex shape("\\bnow\\s*-\\s*[A-Za-z_(][\\w.]*|-\\s*(?:[A-Za-z_]\\w*\\s*(?:\\.|->)\\s*)*since\\b");
        std::vector<Tick> out;
        for (std::sregex_iterator it(code.begin(), code.end(), shape), end; it != end; ++it)
        {
            const size_t line = LineOf(code, static_cast<size_t>(it->position()));
            if (!out.empty() && out.back().line == line) continue;
            out.push_back(Tick{ line, it->str() });
        }
        return out;
    }
}
