#pragma once

#include "guard.h"
#include "lang.h"
#include "policy.h"
#include "registry.h"
#include "verbs.h"
#include "verbs/common.h"

#include <span>

namespace zsdk
{
    namespace ini
    {
        struct Schema;
    }

    struct Identity
    {
        const char *name;
        const char *tag;
        const char *description;
        const char *version;
        const wchar_t *folder;
        const wchar_t *ini;
        const wchar_t *log;
        const wchar_t *rolled;
        const char *registry;
    };

    struct WorldVerbs
    {
        int controller = verbs::common::kPlayerController;
        int pawn = verbs::common::kPlayerPawn;
        int camera = verbs::common::kCameraManager;
        int seconds = verbs::common::kTimeSeconds;
        int paused = verbs::common::kGamePaused;
        int frames = verbs::common::kFrameCount;
        const char *map = "WorldMap_WP";
    };

    struct CensusSpec
    {
        const char *const *statics = nullptr;
        int staticCount = 0;
        const char *const *hosts = nullptr;
        int hostCount = 0;
    };

    struct App
    {
        Identity identity;
        Policy policy;
        std::span<const verbs::Spec> specs;
        WorldVerbs world;
        CensusSpec census;
        std::span<const lang::Row> rows;
        const ini::Schema *schema = nullptr;
        registry::Row row{ nullptr, nullptr, 0.0f, nullptr };
        std::span<const guard::Set *const> sets;
    };

    namespace app
    {
        const App &Get();
        const registry::Row *Row();
        float Lane();
    }

    std::wstring WakeName(const Identity &identity);
    std::wstring MutexShape(const Identity &identity);
}
