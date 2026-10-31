#pragma once

#include <zsdk/zsdk.h>

#include <string>

namespace probe
{
    enum class Look : uint8_t { Cold, Toxic, Count };

    struct Settings
    {
        std::string language = "auto";
        bool enabled = true;
        int logLevel = 1;
        double radiusCm = 150;
        Look look = Look::Cold;
        bool alphaOn = true;
        zsdk::keys::Chord next{ 0xBE, 0 };
        zsdk::keys::Chord back{ 0xBC, 0 };
        std::string callsign = "Strelok";
        std::string mark = "Alpha";
        friend bool operator==(const Settings &, const Settings &) = default;
    };

    inline constexpr const char *kObject = "ObjectProperty";
    inline constexpr const char *kInt = "IntProperty";
    inline constexpr const char *kReal = "DoubleProperty|FloatProperty";
    inline constexpr const char *kFrame = "Int64Property|IntProperty";

    inline constexpr zsdk::verbs::Spec kSpecs[] = {
        { "GameplayStatics", "GetPlayerController", true, { { "WorldContextObject", kObject, 0, 8 }, { "PlayerIndex", kInt, 8, 4 }, { "ReturnValue", kObject, 0x10, 8 } } },
        { "GameplayStatics", "GetPlayerPawn", true, { { "WorldContextObject", kObject, 0, 8 }, { "PlayerIndex", kInt, 8, 4 }, { "ReturnValue", kObject, 0x10, 8 } } },
        { "GameplayStatics", "GetPlayerCameraManager", true, { { "WorldContextObject", kObject, 0, 8 }, { "PlayerIndex", kInt, 8, 4 }, { "ReturnValue", kObject, 0x10, 8 } } },
        { "GameplayStatics", "GetTimeSeconds", true, { { "WorldContextObject", kObject, 0, 8 }, { "ReturnValue", kReal, 8, -1 } } },
        { "KismetSystemLibrary", "GetFrameCount", true, { { "ReturnValue", kFrame, 0, -1 } } },
        { "Actor", "K2_GetActorLocation", false, { { "ReturnValue", "StructProperty", 0, 24 } }, zsdk::verbs::Mark::Risky, 1, 24 },
    };

    static_assert(zsdk::verbs::Counted(kSpecs, true) == 5 && zsdk::verbs::Counted(kSpecs, false) == 1);
    static_assert(zsdk::verbs::Same(kSpecs[5].name, "K2_GetActorLocation"));

    inline constexpr size_t kCallsignMost = 24;

    const zsdk::App &Make();
    zsdk::App &Mutable();
}
