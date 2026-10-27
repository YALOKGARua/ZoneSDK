#include <zsdk/present.h>

#include "hellohud.h"

namespace zhellohud
{
    namespace
    {
        const zsdk::App &Make()
        {
            static const zsdk::App app{
                .identity = { .name = "ZoneHelloHud", .tag = "ZoneHelloHud", .description = HELLOHUD_DESCRIPTION, .version = kVersion, .folder = L"ZoneHelloHud", .ini = L"hellohud.ini", .log = L"hellohud.log", .rolled = L"hellohud.1.log", .registry = "hellohud" },
                .policy = {},
                .specs = kSpecs,
                .world = {},
                .census = { .statics = common::kStatics, .staticCount = static_cast<int>(std::size(common::kStatics)) },
                .rows = Rows(),
                .schema = &Schema(),
                .row = { .id = "hellohud", .tag = "ZoneHelloHud", .lane = 0.66f, .keys = "ALT+7" },
            };
            return app;
        }
    }
}

namespace zsdk::app
{
    const App &Get()
    {
        return zhellohud::Make();
    }
}
