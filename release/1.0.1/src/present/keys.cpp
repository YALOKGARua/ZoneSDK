#include <zsdk/present.h>

#include <windows.h>

#include <cctype>
#include <string>

namespace zsdk::keys
{
    namespace
    {
        struct Named
        {
            uint16_t key;
            const char *name;
            const char *spoken;
        };

        constexpr Named kNamed[] = {
            { VK_INSERT, "INSERT", "Insert" },
            { VK_DELETE, "DELETE", "Delete" },
            { VK_HOME, "HOME", "Home" },
            { VK_END, "END", "End" },
            { VK_PRIOR, "PAGEUP", "PgUp" },
            { VK_NEXT, "PAGEDOWN", "PgDn" },
            { VK_TAB, "TAB", "Tab" },
            { VK_RETURN, "ENTER", "Enter" },
            { VK_SPACE, "SPACE", "Space" },
            { VK_BACK, "BACKSPACE", "Backspace" },
            { VK_UP, "UP", "Up" },
            { VK_DOWN, "DOWN", "Down" },
            { VK_LEFT, "LEFT", "Left" },
            { VK_RIGHT, "RIGHT", "Right" },
            { VK_CAPITAL, "CAPSLOCK", "CapsLock" },
            { VK_SCROLL, "SCROLLLOCK", "ScrollLock" },
            { VK_PAUSE, "PAUSE", "Pause" },
            { VK_MULTIPLY, "NUMMULTIPLY", "Num *" },
            { VK_ADD, "NUMADD", "Num +" },
            { VK_SUBTRACT, "NUMSUBTRACT", "Num -" },
            { VK_DECIMAL, "NUMDECIMAL", "Num ." },
            { VK_DIVIDE, "NUMDIVIDE", "Num /" },
            { VK_OEM_102, "OEM102", "\\ <" },
        };

        struct Symbol
        {
            uint16_t key;
            char text;
            const char *uk;
        };

        constexpr Symbol kSymbols[] = {
            { VK_OEM_PERIOD, '.', ". (Ю)" },
            { VK_OEM_COMMA, ',', ", (Б)" },
            { VK_OEM_MINUS, '-', "-" },
            { VK_OEM_PLUS, '=', "=" },
            { VK_OEM_4, '[', "[ (Х)" },
            { VK_OEM_6, ']', "] (Ї)" },
            { VK_OEM_1, ';', "; (Ж)" },
            { VK_OEM_7, '\'', "' (Є)" },
            { VK_OEM_2, '/', "/ (.)" },
            { VK_OEM_5, '\\', "\\" },
            { VK_OEM_3, '`', "` (')" },
        };

        std::string Clean(std::string_view text)
        {
            size_t from = 0, to = text.size();
            while (from < to && std::isspace(static_cast<unsigned char>(text[from]))) ++from;
            while (to > from && std::isspace(static_cast<unsigned char>(text[to - 1]))) --to;
            std::string out(text.substr(from, to - from));
            for (char &c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return out;
        }

        bool Strip(std::string &rest, std::string_view head)
        {
            if (rest.size() <= head.size() || rest.compare(0, head.size(), head) != 0) return false;
            rest.erase(0, head.size());
            return true;
        }

        const Symbol *SymbolOf(uint16_t key)
        {
            for (const Symbol &s : kSymbols)
                if (s.key == key) return &s;
            return nullptr;
        }

        const Named *NamedOf(uint16_t key)
        {
            for (const Named &n : kNamed)
                if (n.key == key) return &n;
            return nullptr;
        }

        uint16_t Key(const std::string &name)
        {
            if (name.empty() || name == "NONE") return 0;
            if (name.size() == 1)
            {
                const char c = name[0];
                if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return static_cast<uint16_t>(c);
                for (const Symbol &s : kSymbols)
                    if (s.text == c) return s.key;
                return 0;
            }
            for (const Named &n : kNamed)
                if (name == n.name) return n.key;
            if (name.size() >= 2 && name[0] == 'F')
            {
                int number = 0;
                for (size_t i = 1; i < name.size(); ++i)
                {
                    if (name[i] < '0' || name[i] > '9') return 0;
                    number = number * 10 + (name[i] - '0');
                    if (number > 24) return 0;
                }
                return number >= 1 ? static_cast<uint16_t>(VK_F1 + number - 1) : 0;
            }
            if (name.size() == 4 && name.compare(0, 3, "NUM") == 0 && name[3] >= '0' && name[3] <= '9') return static_cast<uint16_t>(VK_NUMPAD0 + (name[3] - '0'));
            return 0;
        }

        std::string Prefix(uint8_t mods, bool upper)
        {
            std::string out;
            if (mods & kCtrl) out += upper ? "CTRL+" : "Ctrl+";
            if (mods & kAlt) out += upper ? "ALT+" : "Alt+";
            if (mods & kShift) out += upper ? "SHIFT+" : "Shift+";
            return out;
        }

        std::string Plain(uint16_t key)
        {
            if (const Symbol *s = SymbolOf(key)) return std::string(1, s->text);
            if (const Named *n = NamedOf(key)) return n->name;
            if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) return std::string(1, static_cast<char>(key));
            if (key >= VK_F1 && key <= VK_F24) return "F" + std::to_string(key - VK_F1 + 1);
            if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return "NUM" + std::to_string(key - VK_NUMPAD0);
            return {};
        }
    }

    bool Known(uint16_t key)
    {
        return !Plain(key).empty();
    }

    Chord Parse(std::string_view text)
    {
        Chord chord;
        std::string rest = Clean(text);
        for (;;)
        {
            if (Strip(rest, "CTRL+") || Strip(rest, "CONTROL+")) chord.mods |= kCtrl;
            else if (Strip(rest, "ALT+")) chord.mods |= kAlt;
            else if (Strip(rest, "SHIFT+")) chord.mods |= kShift;
            else break;
        }
        chord.key = Key(rest);
        if (!chord.key) chord.mods = 0;
        return chord;
    }

    std::string Name(Chord chord)
    {
        if (!chord.key || !Known(chord.key)) return "none";
        return Prefix(chord.mods, true) + Plain(chord.key);
    }

    std::string Glyph(uint16_t key, lang::Lang lang)
    {
        if (!key) return "-";
        if (const Symbol *s = SymbolOf(key)) return lang == lang::Lang::Uk ? std::string(s->uk) : std::string(1, s->text);
        if (const Named *n = NamedOf(key)) return n->spoken;
        if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return "Num " + std::to_string(key - VK_NUMPAD0);
        const std::string plain = Plain(key);
        return plain.empty() ? std::string("?") : plain;
    }

    std::string Shown(Chord chord, lang::Lang lang)
    {
        if (!chord.key) return "-";
        return Prefix(chord.mods, false) + Glyph(chord.key, lang);
    }

    bool Modifier(uint16_t key)
    {
        switch (key)
        {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
        }
    }

    bool Bindable(Chord chord)
    {
        return chord.key != 0 && Known(chord.key) && !Modifier(chord.key) && chord.key != VK_ESCAPE && chord.key != VK_TAB && (chord.mods & ~(kCtrl | kShift | kAlt)) == 0;
    }

    bool Game(uint16_t key)
    {
        static constexpr char kLetters[] = "WASDERFCQVZXBMJPITLHUYOKNG";
        for (const char *walk = kLetters; *walk; ++walk)
            if (key == static_cast<uint16_t>(*walk)) return true;
        return (key >= '1' && key <= '8') || key == VK_SPACE || key == VK_TAB || key == VK_OEM_PLUS || key == VK_F5 || key == VK_F8;
    }

    uint8_t Mods(bool ctrl, bool shift, bool alt)
    {
        return static_cast<uint8_t>((ctrl ? kCtrl : 0) | (shift ? kShift : 0) | (alt ? kAlt : 0));
    }

    bool Match(Chord chord, uint16_t key, uint8_t mods)
    {
        if (chord.key == 0 || chord.key != key) return false;
        if (chord.mods) return chord.mods == mods;
        return (mods & kAlt) == 0;
    }

    bool Numpad(uint16_t key)
    {
        return (key >= VK_NUMPAD0 && key <= VK_DIVIDE) || key == VK_NUMLOCK;
    }

    bool Exact(Chord chord, uint16_t key, uint8_t mods)
    {
        return chord.key != 0 && chord.key == key && chord.mods == mods;
    }

    bool Fires(Chord chord, uint16_t key, uint8_t mods)
    {
        return app::Get().policy.mods == ModRule::Exact ? Exact(chord, key, mods) : Match(chord, key, mods);
    }

    int Which(Chord next, Chord back, uint16_t key, uint8_t mods)
    {
        const bool forward = Match(next, key, mods);
        const bool backward = Match(back, key, mods);
        if (forward && backward)
        {
            if (back.mods == mods && next.mods != mods) return -1;
            return 1;
        }
        return forward ? 1 : backward ? -1 : 0;
    }
}
