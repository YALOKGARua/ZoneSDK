#pragma once

#include "lang.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace zsdk::keys
{
    inline constexpr uint8_t kCtrl = 1;
    inline constexpr uint8_t kShift = 2;
    inline constexpr uint8_t kAlt = 4;

    struct Chord
    {
        uint16_t key = 0;
        uint8_t mods = 0;
        friend bool operator==(const Chord &, const Chord &) = default;
    };

    Chord Parse(std::string_view text);
    std::string Name(Chord chord);
    std::string Glyph(uint16_t key, lang::Lang lang);
    std::string Shown(Chord chord, lang::Lang lang);
    bool Known(uint16_t key);
    bool Modifier(uint16_t key);
    bool Bindable(Chord chord);
    bool Game(uint16_t key);
    bool Numpad(uint16_t key);
    uint8_t Mods(bool ctrl, bool shift, bool alt);
    bool Match(Chord chord, uint16_t key, uint8_t mods);
    bool Exact(Chord chord, uint16_t key, uint8_t mods);
    bool Fires(Chord chord, uint16_t key, uint8_t mods);
    int Which(Chord next, Chord back, uint16_t key, uint8_t mods);
}
