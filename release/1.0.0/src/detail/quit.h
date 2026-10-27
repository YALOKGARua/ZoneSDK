#pragma once

#include <zsdk/guard.h>

#include <iterator>

namespace zsdk::detail::quit
{
    inline constexpr const char *kName = "zsdk.exit";

    inline constexpr guard::Guard kList61bc1e03[] = {
        { 0x41E14AEu, "56 48 83 EC 20 48 89 CE B1 01 E8 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 05 72 ?? 80 3D 5B 21 E0 05 00" },
        { 0x41E14F5u, "C6 05 2C 21 E0 05 01 48 83 C4 20 5E C3" },
    };
    inline constexpr guard::Set kSet61bc1e03{ "zsdk.exit.61bc1e03", &guard::k61bc1e03, kList61bc1e03, 2, guard::Need::ImageSize };
    inline constexpr guard::Field kFlag61bc1e03 = guard::Field::Rip("GIsRequestingExit", 0x9FE3628u, kSet61bc1e03, 1, 2, 7);
    inline constexpr guard::Field kTested61bc1e03 = guard::Field::Rip("GIsRequestingExit.Tested", 0x9FE3628u, kSet61bc1e03, 0, 0x1A, 0x1F);

    inline constexpr guard::Guard kList2bd3f115[] = {
        { 0x4186886u, "56 48 83 EC 20 48 89 CE B1 01 E8 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 05 72 ?? 80 3D 5B 2D E4 05 00" },
        { 0x41868CDu, "C6 05 2C 2D E4 05 01 48 83 C4 20 5E C3" },
    };
    inline constexpr guard::Set kSet2bd3f115{ "zsdk.exit.2bd3f115", &guard::k2bd3f115, kList2bd3f115, 2, guard::Need::ImageSize };
    inline constexpr guard::Field kFlag2bd3f115 = guard::Field::Rip("GIsRequestingExit", 0x9FC9600u, kSet2bd3f115, 1, 2, 7);
    inline constexpr guard::Field kTested2bd3f115 = guard::Field::Rip("GIsRequestingExit.Tested", 0x9FC9600u, kSet2bd3f115, 0, 0x1A, 0x1F);

    struct Exit
    {
        const guard::Set *set;
        const guard::Field *flag;
        const guard::Field *tested;
    };

    inline constexpr Exit kExits[] = {
        { &kSet61bc1e03, &kFlag61bc1e03, &kTested61bc1e03 },
        { &kSet2bd3f115, &kFlag2bd3f115, &kTested2bd3f115 },
    };

    consteval bool Shaped(const Exit &e)
    {
        return e.set->count == 2 && e.set->guards[1].rva == e.set->guards[0].rva + 0x47 && e.flag->set == e.set && e.tested->set == e.set && e.flag->at == e.tested->at &&
               e.flag->proof == guard::Proof::Rip && e.tested->proof == guard::Proof::Rip && e.set->need == guard::Need::ImageSize;
    }

    static_assert(Shaped(kExits[0]) && Shaped(kExits[1]) && std::size(kExits) == std::size(guard::kBuilds));
}
