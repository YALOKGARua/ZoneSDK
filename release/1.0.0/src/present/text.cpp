#include <zsdk/present.h>

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace zsdk::text
{
    namespace
    {
        uint32_t Next(std::string_view utf8, size_t &at)
        {
            const auto lead = static_cast<unsigned char>(utf8[at]);
            int extra = 0;
            uint32_t point = lead;

            if (lead >= 0xF0)
            {
                extra = 3;
                point = lead & 0x07u;
            }
            else if (lead >= 0xE0)
            {
                extra = 2;
                point = lead & 0x0Fu;
            }
            else if (lead >= 0xC0)
            {
                extra = 1;
                point = lead & 0x1Fu;
            }

            ++at;
            for (int i = 0; i < extra && at < utf8.size(); ++i, ++at)
            {
                point = (point << 6) | (static_cast<unsigned char>(utf8[at]) & 0x3Fu);
            }

            return point;
        }

        void Put(std::string &out, uint32_t point)
        {
            if (point < 0x80)
            {
                out.push_back(static_cast<char>(point));
            }
            else if (point < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (point >> 6)));
                out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
            }
            else if (point < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (point >> 12)));
                out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (point >> 18)));
                out.push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
            }
        }
        const char *Skeleton(uint32_t point)
        {
            switch (point)
            {
            case 0x430: return "a";
            case 0x431: return "b";
            case 0x432: return "v";
            case 0x433: case 0x491: case 0x445: return "h";
            case 0x434: return "d";
            case 0x435: case 0x454: case 0x44D: case 0x451: return "e";
            case 0x436: return "j";
            case 0x437: return "z";
            case 0x438: case 0x456: case 0x457: case 0x44B: case 0x439: return "i";
            case 0x43A: return "k";
            case 0x43B: return "l";
            case 0x43C: return "m";
            case 0x43D: return "n";
            case 0x43E: return "o";
            case 0x43F: return "p";
            case 0x440: return "r";
            case 0x441: return "s";
            case 0x442: return "t";
            case 0x443: return "u";
            case 0x444: return "f";
            case 0x446: case 0x447: return "c";
            case 0x448: case 0x449: return "s";
            case 0x44E: return "u";
            case 0x44F: return "a";
            default: return nullptr;
            }
        }

        bool Digit(uint32_t point)
        {
            return point >= '0' && point <= '9';
        }

        bool Times(uint32_t point)
        {
            return point == 'x' || point == 0x445 || point == 0xD7;
        }

        void Replace(std::string &body, const char *from, const char *to)
        {
            const size_t span = strlen(from);
            for (size_t at = body.find(from); at != std::string::npos; at = body.find(from, at + 1))
            {
                body.replace(at, span, to);
            }
        }
    }

    std::string Trim(std::string_view raw)
    {
        size_t from = 0;
        size_t upto = raw.size();
        while (from < upto && (raw[from] == ' ' || raw[from] == '\t' || raw[from] == '\r' || raw[from] == '\n'))
        {
            ++from;
        }
        while (upto > from && (raw[upto - 1] == ' ' || raw[upto - 1] == '\t' || raw[upto - 1] == '\r' || raw[upto - 1] == '\n'))
        {
            --upto;
        }
        return std::string(raw.substr(from, upto - from));
    }

    std::vector<std::string> Split(std::string_view line, char mark)
    {
        std::vector<std::string> parts;
        size_t from = 0;
        for (;;)
        {
            const size_t at = line.find(mark, from);
            if (at == std::string_view::npos)
            {
                parts.emplace_back(line.substr(from));
                return parts;
            }
            parts.emplace_back(line.substr(from, at - from));
            from = at + 1;
        }
    }

    std::vector<std::string> Tokens(std::string_view line)
    {
        std::vector<std::string> out;
        std::string word;
        bool quoted = false;

        for (const char sign : line)
        {
            if (sign == '"')
            {
                quoted = !quoted;
                continue;
            }

            const bool gap = !quoted && (sign == ' ' || sign == '\t' || sign == ',' || sign == '\r' || sign == '\n');
            if (gap)
            {
                if (!word.empty())
                {
                    out.push_back(word);
                    word.clear();
                }
                continue;
            }

            word.push_back(sign);
        }

        if (!word.empty())
        {
            out.push_back(word);
        }
        return out;
    }

    std::vector<std::string> Lines(const std::string &body)
    {
        std::vector<std::string> out;
        size_t from = 0;
        if (body.size() >= 3 && static_cast<unsigned char>(body[0]) == 0xEF && static_cast<unsigned char>(body[1]) == 0xBB &&
            static_cast<unsigned char>(body[2]) == 0xBF)
        {
            from = 3;
        }

        while (from <= body.size())
        {
            size_t at = body.find('\n', from);
            if (at == std::string::npos)
            {
                at = body.size();
            }
            size_t upto = at;
            if (upto > from && body[upto - 1] == '\r')
            {
                --upto;
            }
            out.emplace_back(body.substr(from, upto - from));
            from = at + 1;
        }

        return out;
    }

    std::wstring Wide(std::string_view utf8)
    {
        if (utf8.empty())
        {
            return {};
        }
        const int need = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(need > 0 ? need : 0), L'\0');
        if (need > 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), need);
        }
        return out;
    }

    std::string Narrow(std::wstring_view wide)
    {
        if (wide.empty())
        {
            return {};
        }
        const int need = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(need > 0 ? need : 0), '\0');
        if (need > 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), need, nullptr, nullptr);
        }
        return out;
    }

    uint32_t Fold(uint32_t point)
    {
        if (point >= 'A' && point <= 'Z')
        {
            return point + 32;
        }
        if (point >= 0x410 && point <= 0x42F)
        {
            return point + 0x20;
        }
        if (point >= 0x400 && point <= 0x40F)
        {
            return point + 0x50;
        }
        if (point == 0x490)
        {
            return 0x491;
        }
        return point;
    }

    std::string Lower(std::string_view utf8)
    {
        std::string out;
        out.reserve(utf8.size());
        for (size_t at = 0; at < utf8.size();)
        {
            Put(out, Fold(Next(utf8, at)));
        }
        return out;
    }

    uint32_t Raise(uint32_t point)
    {
        if (point >= 'a' && point <= 'z')
        {
            return point - 32;
        }
        if (point >= 0x430 && point <= 0x44F)
        {
            return point - 0x20;
        }
        if (point >= 0x450 && point <= 0x45F)
        {
            return point - 0x50;
        }
        if (point == 0x491)
        {
            return 0x490;
        }
        return point;
    }

    std::string Upper(std::string_view utf8)
    {
        std::string out;
        out.reserve(utf8.size());
        for (size_t at = 0; at < utf8.size();)
        {
            Put(out, Raise(Next(utf8, at)));
        }
        return out;
    }

    std::string Latin(std::string_view utf8)
    {
        std::string body;
        body.reserve(utf8.size());

        uint32_t before = 0;
        for (size_t at = 0; at < utf8.size();)
        {
            const uint32_t point = Fold(Next(utf8, at));
            const uint32_t prior = before;
            before = point;
            if (Times(point))
            {
                size_t ahead = at;
                while (ahead < utf8.size() && utf8[ahead] == '-')
                {
                    ++ahead;
                }
                if (Digit(prior) || (ahead < utf8.size() && Digit(Next(utf8, ahead))))
                {
                    body.push_back('*');
                    continue;
                }
            }

            if ((point >= 'a' && point <= 'z') || (point >= '0' && point <= '9'))
            {
                body.push_back(static_cast<char>(point));
                continue;
            }

            if (const char *skeleton = Skeleton(point))
            {
                body += skeleton;
            }
        }

        static const char *const pairs[] = { "shch", "s", "sch", "s", "zh", "j", "kh", "h", "ch", "c", "sh", "s",
                                             "ts", "c", "ya", "a", "yu", "u", "ye", "e", "yi", "i" };
        for (size_t i = 0; i + 1 < sizeof(pairs) / sizeof(pairs[0]); i += 2)
        {
            Replace(body, pairs[i], pairs[i + 1]);
        }

        std::string out;
        out.reserve(body.size());
        char last = '\0';
        for (const char sign : body)
        {
            char mapped = sign;
            switch (sign)
            {
            case 'y': mapped = 'i'; break;
            case 'w': mapped = 'v'; break;
            case 'q': mapped = 'k'; break;
            case 'g': mapped = 'h'; break;
            case 'x':
                out += "ks";
                last = 's';
                continue;
            default: break;
            }

            if (mapped == last && mapped >= 'a' && mapped <= 'z')
            {
                continue;
            }
            out.push_back(mapped);
            last = mapped;
        }

        return out;
    }

    bool Cyrillic(std::string_view utf8)
    {
        for (size_t at = 0; at < utf8.size();)
        {
            const uint32_t point = Next(utf8, at);
            if (point >= 0x400 && point <= 0x4FF)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<uint32_t> Points(std::string_view utf8)
    {
        std::vector<uint32_t> out;
        for (size_t at = 0; at < utf8.size();)
        {
            out.push_back(Next(utf8, at));
        }
        return out;
    }

    bool Starts(std::string_view text, std::string_view head)
    {
        return text.size() >= head.size() && text.compare(0, head.size(), head) == 0;
    }

    bool Ends(std::string_view text, std::string_view tail)
    {
        return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
    }

    bool Same(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (size_t i = 0; i < left.size(); ++i)
        {
            if (Fold(static_cast<unsigned char>(left[i])) != Fold(static_cast<unsigned char>(right[i])))
            {
                return false;
            }
        }
        return true;
    }

    bool Color(std::string_view token, uint32_t &argb)
    {
        if (token.size() < 7 || token[0] != '#')
        {
            return false;
        }

        uint32_t value = 0;
        for (size_t i = 1; i < 7; ++i)
        {
            const char sign = token[i];
            uint32_t digit = 0;
            if (sign >= '0' && sign <= '9')
            {
                digit = static_cast<uint32_t>(sign - '0');
            }
            else if (sign >= 'a' && sign <= 'f')
            {
                digit = static_cast<uint32_t>(sign - 'a' + 10);
            }
            else if (sign >= 'A' && sign <= 'F')
            {
                digit = static_cast<uint32_t>(sign - 'A' + 10);
            }
            else
            {
                return false;
            }
            value = (value << 4) | digit;
        }

        argb = 0xFF000000u | value;
        return true;
    }

    std::string Format(const char *shape, ...)
    {
        char small[512]{};
        va_list rest;
        va_start(rest, shape);
        const int need = _vsnprintf_s(small, sizeof(small), _TRUNCATE, shape, rest);
        va_end(rest);

        if (need >= 0)
        {
            return std::string(small, static_cast<size_t>(need));
        }

        va_start(rest, shape);
        const int full = _vscprintf(shape, rest);
        va_end(rest);

        std::string out(static_cast<size_t>(full > 0 ? full : 0), '\0');
        if (full > 0)
        {
            va_start(rest, shape);
            vsnprintf_s(out.data(), out.size() + 1, _TRUNCATE, shape, rest);
            va_end(rest);
        }
        return out;
    }

    std::string Read(const std::string &path)
    {
        FILE *file = nullptr;
        if (_wfopen_s(&file, Wide(path).c_str(), L"rb") != 0 || file == nullptr)
        {
            return {};
        }

        std::string body;
        char chunk[65536];
        for (;;)
        {
            const size_t got = fread(chunk, 1, sizeof(chunk), file);
            if (got == 0)
            {
                break;
            }
            body.append(chunk, got);
        }

        fclose(file);
        return body;
    }

    bool Write(const std::string &path, const std::string &body)
    {
        FILE *file = nullptr;
        if (_wfopen_s(&file, Wide(path).c_str(), L"wb") != 0 || file == nullptr)
        {
            return false;
        }

        const bool fine = fwrite(body.data(), 1, body.size(), file) == body.size();
        fclose(file);
        return fine;
    }
}
