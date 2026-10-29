#pragma once

#include "../verbs.h"

#include <array>
#include <cstddef>
#include <span>

namespace zsdk::verbs
{
    template<size_t A, size_t B> constexpr std::array<Spec, A + B> Join(const Spec (&a)[A], const Spec (&b)[B])
    {
        std::array<Spec, A + B> out{};
        for (size_t i = 0; i < A; ++i) out[i] = a[i];
        for (size_t i = 0; i < B; ++i) out[A + i] = b[i];
        return out;
    }

    template<size_t A, size_t B> constexpr std::array<Spec, A + B> Join(const std::array<Spec, A> &a, const Spec (&b)[B])
    {
        std::array<Spec, A + B> out{};
        for (size_t i = 0; i < A; ++i) out[i] = a[i];
        for (size_t i = 0; i < B; ++i) out[A + i] = b[i];
        return out;
    }

    template<size_t N> constexpr int Counted(const std::array<Spec, N> &specs, bool required)
    {
        int n = 0;
        for (const Spec &spec : specs)
            if (spec.name && spec.required == required) ++n;
        return n;
    }

    constexpr int Index(std::span<const Spec> specs, const char *owner, const char *name)
    {
        for (size_t i = 0; i < specs.size(); ++i)
            if (specs[i].name && specs[i].owner && Same(specs[i].name, name) && (!owner || Same(specs[i].owner, owner))) return static_cast<int>(i);
        return -1;
    }

    constexpr int Index(std::span<const Spec> specs, const char *name)
    {
        return Index(specs, nullptr, name);
    }
}

namespace zsdk::verbs::common
{
    inline constexpr const char *kObject = "ObjectProperty";
    inline constexpr const char *kInt = "IntProperty";
    inline constexpr const char *kFloat = "FloatProperty";
    inline constexpr const char *kReal = "DoubleProperty|FloatProperty";
    inline constexpr const char *kFrame = "Int64Property|IntProperty";
    inline constexpr const char *kBool = "BoolProperty";
    inline constexpr const char *kStr = "StrProperty";
    inline constexpr const char *kEnum = "EnumProperty|ByteProperty";
    inline constexpr const char *kStruct = "StructProperty";

    enum World : int { kPlayerController, kPlayerPawn, kCameraManager, kTimeSeconds, kGamePaused, kFrameCount, kWorldCount };

    inline constexpr Spec kWorld[kWorldCount] = {
        { "GameplayStatics", "GetPlayerController", true, { { "WorldContextObject", kObject, 0, 8 }, { "PlayerIndex", kInt, 8, 4 }, { "ReturnValue", kObject, 0x10, 8 } } },
        { "GameplayStatics", "GetPlayerPawn", true, { { "WorldContextObject", kObject, 0, 8 }, { "PlayerIndex", kInt, 8, 4 }, { "ReturnValue", kObject, 0x10, 8 } } },
        { "GameplayStatics", "GetPlayerCameraManager", true, { { "WorldContextObject", kObject, 0, 8 }, { "PlayerIndex", kInt, 8, 4 }, { "ReturnValue", kObject, 0x10, 8 } } },
        { "GameplayStatics", "GetTimeSeconds", true, { { "WorldContextObject", kObject, 0, 8 }, { "ReturnValue", kReal, 8, -1 } } },
        { "GameplayStatics", "IsGamePaused", false, { { "WorldContextObject", kObject, 0, 8 }, { "ReturnValue", kBool, 8, 1 } } },
        { "KismetSystemLibrary", "GetFrameCount", true, { { "ReturnValue", kFrame, 0, -1 } } },
    };

    constexpr bool Plays(std::span<const Spec> specs, int verb, World role)
    {
        if (verb < 0 || static_cast<size_t>(verb) >= specs.size() || role < 0 || role >= kWorldCount) return false;
        const Spec &spec = specs[static_cast<size_t>(verb)];
        const Spec &want = kWorld[role];
        return spec.owner && spec.name && Same(spec.owner, want.owner) && Same(spec.name, want.name);
    }

    inline constexpr const char *kStatics[] = { "GameplayStatics", "KismetSystemLibrary" };

    enum Screen : int { kInCinematic, kInStaticDialog, kUsingPda, kUsingBackpack, kInInteraction, kScreenCount };

    inline constexpr Spec kScreen[kScreenCount] = {
        { "Obj", "IsInCinematic", false, { { "ReturnValue", kBool, 0, 1 } } },
        { "PC", "IsInStaticDialog", false, { { "ReturnValue", kBool, 0, 1 } } },
        { "PC", "IsUsingPDA", false, { { "ReturnValue", kBool, 0, 1 } } },
        { "PC", "IsUsingBackpack", false, { { "ReturnValue", kBool, 0, 1 } } },
        { "PC", "IsInteractionInProgress", false, { { "ReturnValue", kBool, 0, 1 } } },
    };

    enum Console : int { kDestroyModel, kNeedsValue, kConsoleCount };

    inline constexpr const char *kConsoleHost = "CustomConsoleManagerAI2";

    inline constexpr Spec kConsole[kConsoleCount] = {
        { kConsoleHost, "XDestroyModel", false, { { "UID", kInt, 0, 4 } }, Mark::Risky },
        { kConsoleHost, "XNeedsSetNeedsValue", false, { { "Need", kStr, 0, 16 }, { "Value", kInt, 0x10, 4 }, { "UID", kInt, 0x14, 4 } }, Mark::Risky },
    };

    static_assert(Counted(kWorld, true) == 5 && Counted(kWorld, false) == 1);
    static_assert(Same(kWorld[kGamePaused].name, "IsGamePaused") && Same(kWorld[kFrameCount].name, "GetFrameCount") && Same(kWorld[kCameraManager].name, "GetPlayerCameraManager"));
    static_assert(Same(kScreen[kInStaticDialog].name, "IsInStaticDialog") && Same(kScreen[kInInteraction].name, "IsInteractionInProgress"));
    static_assert(Same(kConsole[kNeedsValue].name, "XNeedsSetNeedsValue"));
}
