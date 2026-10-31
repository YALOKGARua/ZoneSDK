#pragma once

#include <zsdk/keys.h>

namespace zsdk::detail::keys
{
    inline bool Clash(zsdk::keys::Chord chord)
    {
        return chord.key != 0 && zsdk::keys::Game(chord.key) && (chord.mods & zsdk::keys::kAlt) == 0;
    }
}
