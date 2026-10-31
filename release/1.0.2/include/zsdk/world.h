#pragma once

#include "math.h"

#include <cstdint>
#include <string>

namespace zsdk::world
{
    struct Grip
    {
        const void *gi = nullptr;
        const void *giType = nullptr;
        const void *pilot = nullptr;
        const void *body = nullptr;
        const void *bodyType = nullptr;
        const void *lens = nullptr;
        const void *lensType = nullptr;
        const void *world = nullptr;
        const void *home = nullptr;
        const void *worldType = nullptr;
        std::string worldName;
        int32_t povAt = -1;
        double seconds = -1;
        int64_t frame = -1;
        bool frameless = false;
        bool growing = false;
        bool fit = false;
        bool stable = false;
        bool paused = false;
        bool first = true;
        uint64_t since = 0;
        uint64_t polled = 0;
        uint64_t probed = 0;
        uint32_t epoch = 0;
        const char *wait = "wait.world";
        uint64_t at = 0;
    };

    struct Turn
    {
        const char *why;
        const void *world;
        bool sameWorld;
        uint32_t epoch;
    };

    Grip &Hold();
    Grip Snapshot();
    bool Paused();
    uint32_t Epoch();
    const void *WorldOf(const void *thing);
    const void *OwningWorld(const void *thing);
    bool Look(const void *lens, const void *type, int32_t at, math::Vec3 &loc, math::Rot3 &rot);
    int32_t PovAt(const void *lens, const void *type);
    const void *Player(int verb, const void *context);
    double Clock(const void *context);
    bool OpenWorld();
    bool Paused(const void *context);
}
