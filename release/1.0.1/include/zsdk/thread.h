#pragma once

#include <cstdint>

namespace zsdk
{
    enum class Phase : uint8_t { Off, Hooking, Binding, Wiring, Waiting, Ready, Failed, Halted, Lost, Count };

    namespace thread
    {
        struct Counters
        {
            uint64_t posted = 0;
            uint64_t seen = 0;
            uint64_t ticked = 0;
            uint64_t pumpAt = 0;
            uint64_t turns = 0;
            int wires = 0;
            double wireMs = 0;
        };

        bool OnGame();
        bool Started();
        bool Hooked();
        bool Bound();
        bool Booked();
        bool Rejected();
        bool Wired();
        bool Halted();
        bool Stopping();
        bool Overlaid();
        void Wake();
        void Ask(int order);
        void Knock(uint32_t ms);
        double LastMs();
        double WorstMs();
        uint64_t LastPumpAt();
        bool PumpAlive();
        Counters Count();
        Phase State(bool stable);
    }
}
