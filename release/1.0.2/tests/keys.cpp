#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include <windows.h>

#include "../src/detail/keys.h"

#include <map>
#include <string>

int main()
{
    using check::That;
    namespace keys = zsdk::keys;
    using keys::Chord;
    using L = zsdk::lang::Lang;
    That(zsdk::detail::keys::Clash(keys::Parse("H")) && zsdk::detail::keys::Clash(keys::Parse("CTRL+H")) && zsdk::detail::keys::Clash(keys::Parse("SHIFT+W")), "the tab warns that the game uses a plain, CTRL or SHIFT chord of a game key");
    That(!zsdk::detail::keys::Clash(keys::Parse("ALT+H")) && !zsdk::detail::keys::Clash(keys::Parse("CTRL+ALT+W")) && !zsdk::detail::keys::Clash(keys::Parse("F7")) && !zsdk::detail::keys::Clash(Chord{}), "an ALT chord of a game key, a key the game does not use and no key give no game-key warning");
    That(keys::Parse(".").key == VK_OEM_PERIOD && keys::Parse(",").key == VK_OEM_COMMA, ". and , parse");
    That(keys::Parse("ctrl+shift+]").mods == (keys::kCtrl | keys::kShift) && keys::Parse("ctrl+shift+]").key == VK_OEM_6, "modifiers parse in any case");
    That(keys::Parse("F24").key == VK_F24 && keys::Parse("F25").key == 0 && keys::Parse("NUM7").key == VK_NUMPAD7, "function and numpad keys");
    That(keys::Parse("none").key == 0 && keys::Parse("ctrl+").mods == 0, "none and a lone modifier are no key");
    for (const char *text : { "CTRL+]", "ALT+F9", "SHIFT+.", "INSERT", "NUMADD", "Q", "7", "F12" }) That(keys::Name(keys::Parse(text)) == text, std::string("name round trip ") + text);
    That(keys::Name(Chord{}) == "none" && keys::Name(Chord{ 0x07, 0 }) == "none" && keys::Name(Chord{ 0xE8, 0 }) == "none", "an unknown key is none, never INSERT");
    That(keys::Shown(Chord{ VK_OEM_PERIOD, 0 }, L::Uk) == ". (Ю)" && keys::Shown(Chord{ VK_OEM_7, 0 }, L::Uk) == "' (Є)" && keys::Shown(Chord{ VK_OEM_PERIOD, 0 }, L::En) == ".",
         "layout glyphs");
    That(keys::Shown(Chord{ VK_ADD, keys::kShift }, L::En) == "Shift+Num +" && keys::Shown(Chord{}, L::Uk) == "-", "spoken names");
    That(keys::Bindable(Chord{ VK_F9, keys::kAlt }) && !keys::Bindable(Chord{ VK_ESCAPE, 0 }) && !keys::Bindable(Chord{ VK_TAB, 0 }) && !keys::Bindable(Chord{ VK_LSHIFT, 0 }),
         "bindable keys");
    That(keys::Numpad(VK_NUMPAD0) && keys::Numpad(VK_ADD) && !keys::Numpad(VK_OEM_PLUS), "numpad keys are recognised");
    const Chord dot{ VK_OEM_PERIOD, 0 };
    const Chord shifted{ VK_OEM_PERIOD, keys::kShift };
    That(keys::Match(dot, VK_OEM_PERIOD, keys::kShift) && keys::Match(dot, VK_OEM_PERIOD, keys::kCtrl) && !keys::Match(dot, VK_OEM_PERIOD, keys::kAlt), "the Alt rule");
    That(keys::Match(shifted, VK_OEM_PERIOD, keys::kShift) && !keys::Match(shifted, VK_OEM_PERIOD, 0), "a chord needs its exact modifiers");
    That(keys::Exact(dot, VK_OEM_PERIOD, 0) && !keys::Exact(dot, VK_OEM_PERIOD, keys::kShift), "the exact rule");
    That(keys::Fires(dot, VK_OEM_PERIOD, keys::kShift), "the default policy fires with the Alt rule");
    That(keys::Which(dot, Chord{ VK_OEM_COMMA, 0 }, VK_OEM_COMMA, 0) == -1 && keys::Which(dot, shifted, VK_OEM_PERIOD, keys::kShift) == -1 && keys::Which(dot, shifted, VK_OEM_PERIOD, 0) == 1,
         "which action a key belongs to");
    std::map<std::string, std::string> chords;
    int keyed = 0;
    for (const auto &row : zsdk::registry::Rows())
        for (const auto &one : zsdk::text::Split(row.keys, '|'))
            if (!one.empty())
            {
                ++keyed;
                const Chord c = keys::Parse(one);
                That(c.key != 0, std::string("registry key parses: ") + row.id + " " + one);
                That(!keys::Numpad(c.key), std::string("no numpad defaults: ") + row.id + " " + one);
                if (!c.mods) continue;
                const auto [at, fresh] = chords.emplace(keys::Name(c), row.id);
                That(fresh || at->second == row.id, std::string("a modifier chord belongs to one registry row: ") + keys::Name(c) + " " + row.id + " " + at->second);
            }
    That(keyed >= 15 && chords.count("ALT+J") && chords.at("ALT+J") == "pulse" && chords.count("ALT+L") && chords.at("ALT+L") == "template", "the table carries the released default keys, " + std::to_string(keyed));
    return check::Finish("keys");
}
