#pragma once

#include <zsdk/guard.h>

namespace zfieldwatch::fields
{
    namespace guard = zsdk::guard;

    inline constexpr const char *kCameraName = "fieldwatch.camera";

    inline constexpr guard::Guard kCameraGuards61bc1e03[] = {
        { .rva = 0x23CA0E4, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 0F 57 C9 0F 2E C1 77 22 48 8B 01 48 8B 80 A0 07 00 00 48 8D 15 ?? ?? ?? ?? 48 39 D0 75 11 48 81 C1 C0 17 00 00 F3 0F 10 41 30" },
    };
    inline constexpr guard::Guard kCameraGuards2bd3f115[] = {
        { .rva = 0x22B5CDE, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 0F 57 C9 0F 2E C1 77 22 48 8B 01 48 8B 80 A0 07 00 00 48 8D 15 ?? ?? ?? ?? 48 39 D0 75 11 48 81 C1 C0 17 00 00 F3 0F 10 41 30" },
    };

    inline constexpr guard::Set kCamera61bc1e03{ "fieldwatch.camera.61bc1e03", &guard::k61bc1e03, kCameraGuards61bc1e03, 1, guard::Need::KnownBuild };
    inline constexpr guard::Set kCamera2bd3f115{ "fieldwatch.camera.2bd3f115", &guard::k2bd3f115, kCameraGuards2bd3f115, 1, guard::Need::KnownBuild };

    inline constexpr guard::Field kDefaultFov{ "Camera.DefaultFOV", 0x2C8, "PlayerCameraManager", "DefaultFOV" };
    inline constexpr guard::Field kLockedFov61bc1e03{ "Camera.LockedFOV", 0x2CC, kCamera61bc1e03, 0, 8 };
    inline constexpr guard::Field kLockedFov2bd3f115{ "Camera.LockedFOV", 0x2CC, kCamera2bd3f115, 0, 8 };
    inline constexpr guard::Field kViewFov = guard::Field::Unproven("Camera.ViewFOV", 0x17F0);

    inline constexpr const guard::Set *kSets[] = { &kCamera61bc1e03, &kCamera2bd3f115 };
    inline constexpr const guard::Field *kLockedFovs[] = { &kLockedFov61bc1e03, &kLockedFov2bd3f115 };

    static_assert(kDefaultFov.proof == guard::Proof::Reflected && kLockedFov61bc1e03.proof == guard::Proof::Guard && kLockedFov2bd3f115.proof == guard::Proof::Guard && kViewFov.proof == guard::Proof::Unproven);
    static_assert(kLockedFov61bc1e03.set == &kCamera61bc1e03 && kCamera61bc1e03.need == guard::Need::KnownBuild && kCamera61bc1e03.build == &guard::k61bc1e03);
    static_assert(kLockedFov2bd3f115.set == &kCamera2bd3f115 && kCamera2bd3f115.need == guard::Need::KnownBuild && kCamera2bd3f115.build == &guard::k2bd3f115);
}
