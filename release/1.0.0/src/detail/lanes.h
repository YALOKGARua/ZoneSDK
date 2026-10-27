#pragma once

#include <cstdint>
#include <string>

namespace zsdk::detail::lanes
{
    enum Lane : uint32_t { Boot, Core, Frame, Data, Block, Overlay, Toasts, Page, Open, Device, DeviceDestroy, Runtime, Save, PipelineInit, PipelineBind, PipelineDestroy, Count };

    inline constexpr const char *kTag[Count] = { "boot", "core", "frame", "data", "block", "overlay", "toasts", "page", "open", "device", "device destroy", "runtime", "save", "pipeline init",
                                                 "pipeline bind", "pipeline destroy" };
    inline constexpr const char *kKey[Count] = { "present.boot", "present.core", "present.frame", "present.data", "present.block", "present.overlay", "present.toasts", "present.page",
                                                 "present.open", "present.device", "present.deviceDestroy", "present.runtime", "present.save", "present.pipelineInit", "present.pipelineBind",
                                                 "present.pipelineDestroy" };
    inline constexpr const char *kRow[Count] = { "lane.boot", "lane.core", "lane.frame", "lane.data", "lane.block", "lane.overlay", "lane.toasts", "lane.page", "lane.open", "lane.device", "lane.deviceDestroy",
                                                 "lane.screen", "lane.save", "lane.pipelineInit", "lane.pipelineBind", "lane.pipelineDestroy" };

    constexpr uint32_t Bit(Lane lane)
    {
        return lane < Count ? 1u << lane : 0u;
    }

    inline constexpr uint32_t kAll = (1u << Count) - 1u;
    inline constexpr uint32_t kShared = Bit(Frame) | Bit(Toasts);
    inline constexpr uint32_t kStart = Bit(Boot) | Bit(Core);
    inline constexpr uint32_t kPark = kAll & ~(Bit(Device) | Bit(Runtime) | Bit(Save));

    constexpr uint32_t Spread(Lane lane)
    {
        if (Bit(lane) & kStart) return kPark;
        if (Bit(lane) & kShared) return kShared;
        return Bit(lane);
    }

    constexpr uint32_t Restore(uint32_t blocked)
    {
        blocked &= kAll;
        return (blocked & kStart) ? (kPark | blocked) : blocked;
    }

    constexpr uint32_t Off(uint32_t blocked, uint32_t fresh)
    {
        uint32_t off = Restore(blocked);
        if (fresh & kStart) off |= kPark;
        for (uint32_t i = 0; i < Count; ++i)
            if (blocked & fresh & (1u << i)) off |= Spread(static_cast<Lane>(i));
        return off;
    }

    inline std::string Names(uint32_t bits)
    {
        std::string out;
        for (uint32_t i = 0; i < Count; ++i)
        {
            if (!(bits & (1u << i))) continue;
            if (!out.empty()) out += ", ";
            out += kTag[i];
        }
        return out;
    }
}
