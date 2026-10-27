#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace zsdk::census
{
    inline constexpr size_t kSlots = 16;
    inline constexpr size_t kInstanceCap = 8;

    struct Snapshot
    {
        uint32_t epoch = 0;
        uint64_t at = 0;
        const void *gi = nullptr;
        std::vector<const void *> gis;
        std::array<const void *, kSlots> statics{};
        std::array<const void *, kSlots> hosts{};
        double sweepMs = 0;
        int objects = 0;
    };

    std::shared_ptr<const Snapshot> Current();
    void Want(const char *reason);
    uint32_t Epoch();
    int Sweeps();
    bool Fresh(const Snapshot &snapshot, uint32_t epoch, uint64_t since);
    const void *Host(size_t slot);
    bool Consoles();
}
