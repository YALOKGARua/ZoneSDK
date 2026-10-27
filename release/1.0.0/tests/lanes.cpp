#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include "../src/detail/lanes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using check::That;
    namespace lanes = zsdk::detail::lanes;

    constexpr uint32_t kTidy = lanes::Bit(lanes::Device) | lanes::Bit(lanes::Runtime) | lanes::Bit(lanes::Save);

    bool Within(uint32_t small, uint32_t big)
    {
        return (small & ~big) == 0;
    }

    uint32_t Spreads(uint32_t bits)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < lanes::Count; ++i)
            if (bits & (1u << i)) out |= lanes::Spread(static_cast<lanes::Lane>(i));
        return out;
    }

    void Restores()
    {
        int wrong = 0;
        for (uint32_t blocked = 0; blocked <= lanes::kAll; ++blocked)
        {
            const uint32_t r = lanes::Restore(blocked);
            const bool fine = Within(r, lanes::kAll) && Within(blocked, r) && Within(r & kTidy, blocked) && ((blocked & lanes::kStart) ? Within(lanes::kPark, r) : r == blocked);
            if (!fine) ++wrong;
        }
        That(wrong == 0, "for every one of the 65536 blocked sets a restore keeps exactly the blocked lanes, parks the add-on after a boot or core fault and never parks device, runtime or save: " + std::to_string(wrong) + " wrong");
        That(lanes::Restore(0xFFFFFFFFu) == lanes::kAll, "a restore ignores bits beyond the lanes");
    }

    void Offs()
    {
        std::vector<uint32_t> freshes = { 0, lanes::kStart, lanes::kShared, lanes::kPark, lanes::kAll };
        for (uint32_t i = 0; i < lanes::Count; ++i) freshes.push_back(1u << i);
        int wrong = 0;
        int tidy = 0;
        int back = 0;
        for (uint32_t blocked = 0; blocked <= lanes::kAll; ++blocked)
            for (const uint32_t fresh : freshes)
            {
                const uint32_t off = lanes::Off(blocked, fresh);
                const uint32_t restored = lanes::Restore(blocked);
                const uint32_t spread = Spreads(blocked & fresh);
                const uint32_t floor = restored | ((fresh & lanes::kStart) ? lanes::kPark : 0u) | spread;
                if (off != floor || !Within(off, lanes::kAll)) ++wrong;
                if (!Within(off & kTidy, blocked)) ++tidy;
                const uint32_t cleared = fresh & ~blocked;
                for (uint32_t k = 0; k < lanes::Count; ++k)
                {
                    const uint32_t bit = 1u << k;
                    if (!(cleared & bit) || (fresh & lanes::kStart) || (spread & bit) || (restored & bit)) continue;
                    if (off & bit) ++back;
                }
            }
        That(wrong == 0, "Try again keeps the restored lanes, the park after a boot or core fault of this session and the spread of every lane still blocked, nothing more: " + std::to_string(wrong) + " wrong");
        That(tidy == 0, "Try again never leaves device, runtime or save off unless that lane itself is blocked: " + std::to_string(tidy) + " wrong");
        That(back == 0, "a lane that faulted this session and is cleared by Try again runs again unless a kept rule holds it: " + std::to_string(back) + " wrong");
        int grows = 0;
        for (uint32_t blocked = 0; blocked <= lanes::kAll; blocked += 7)
            for (uint32_t more = 0; more < lanes::Count; ++more)
                for (const uint32_t fresh : freshes)
                {
                    const uint32_t wider = blocked | (1u << more);
                    if (!Within(lanes::Off(blocked, fresh), lanes::Off(wider, fresh)) || !Within(lanes::Off(blocked, fresh), lanes::Off(blocked, fresh | (1u << more)))) ++grows;
                }
        That(grows == 0, "Off only grows when a lane is added to the blocked or the fresh set: " + std::to_string(grows) + " wrong");
    }

    void Spreading()
    {
        int wrong = 0;
        for (uint32_t i = 0; i < lanes::Count; ++i)
        {
            const lanes::Lane lane = static_cast<lanes::Lane>(i);
            const uint32_t s = lanes::Spread(lane);
            if (!(s & lanes::Bit(lane)) || !Within(s, lanes::kAll) || (s & kTidy & ~lanes::Bit(lane))) ++wrong;
        }
        That(wrong == 0, "every lane switches off at least itself and never takes device, runtime or save with it");
        That(lanes::Spread(lanes::Count) == 0 && lanes::Bit(lanes::Count) == 0, "a lane number out of range switches nothing");
        That(lanes::Names(lanes::kAll) == "boot, core, frame, data, block, overlay, toasts, page, open, device, device destroy, runtime, save, pipeline init, pipeline bind, pipeline destroy", "the lane list reads in lane order: " + lanes::Names(lanes::kAll));
        That((lanes::kPark & lanes::Bit(lanes::DeviceDestroy)) && (lanes::kPark & lanes::Bit(lanes::PipelineInit)) && (lanes::kPark & lanes::Bit(lanes::Open)), "a parked add-on runs none of its own device, pipeline or open hooks");
        That(lanes::kShared == (lanes::Bit(lanes::Frame) | lanes::Bit(lanes::Toasts)), "only frame and toasts go off together, both run add-on code over the same add-on state");
        That(!(lanes::Spread(lanes::Frame) & lanes::Bit(lanes::Data)) && !(lanes::Spread(lanes::Toasts) & lanes::Bit(lanes::Data)), "a frame or toasts fault leaves the data lane on, it runs only SDK code (the log flush and the settings save)");
        int rows = 0;
        for (uint32_t i = 0; i < lanes::Count; ++i)
            if (std::string(lanes::kKey[i]).rfind("present.", 0) == 0 && std::string(lanes::kRow[i]).rfind("lane.", 0) == 0 && zsdk::lang::Text(lanes::kRow[i], zsdk::lang::Lang::En) != std::string("?") && zsdk::lang::Text(lanes::kRow[i], zsdk::lang::Lang::Uk) != std::string("?")) ++rows;
        That(rows == static_cast<int>(lanes::Count), "every lane has a present.<lane> key and an SDK lang row with its UA and EN name for the fallback tab: " + std::to_string(rows));
    }
}

int main()
{
    Restores();
    Offs();
    Spreading();
    return check::Finish("lanes");
}
