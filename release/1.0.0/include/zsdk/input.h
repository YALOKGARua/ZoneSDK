#pragma once

#include "keys.h"

#include <cstdint>

namespace zsdk
{
    struct InputAccess;

    class Input
    {
    public:
        bool Down(uint32_t vk) const;
        bool Pressed(uint32_t vk) const;
        bool Released(uint32_t vk) const;
        bool MouseDown(int button) const;
        bool MousePressed(int button) const;
        void Cursor(uint32_t &x, uint32_t &y, int16_t *wheel = nullptr) const;
        bool Async(uint32_t vk) const;
        uint8_t Mods() const;
        bool Fires(keys::Chord chord) const;
        bool Focused() const;
        bool Overlaid() const;
        bool Menu() const;
        bool Paused() const;

    private:
        void *runtime = nullptr;
        bool focused = false;
        bool overlaid = false;
        bool menu = false;
        bool paused = false;
        friend struct InputAccess;
    };
}
