#include <zsdk/present.h>

#include "fieldwatch.h"

#include <iterator>

namespace zfieldwatch
{
    namespace
    {
        namespace common = zsdk::verbs::common;

        const zsdk::App &Make()
        {
            static const zsdk::App app{
                .identity = { .name = "ZoneFieldWatch", .tag = "ZoneFieldWatch", .description = FIELDWATCH_DESCRIPTION, .version = kVersion, .folder = L"ZoneFieldWatch", .ini = L"fieldwatch.ini", .log = L"fieldwatch.log", .rolled = L"fieldwatch.1.log", .registry = "fieldwatch" },
                .policy = {},
                .specs = common::kWorld,
                .world = {},
                .census = { .statics = common::kStatics, .staticCount = static_cast<int>(std::size(common::kStatics)) },
                .rows = Rows(),
                .schema = &Schema(),
                .row = { .id = "fieldwatch", .tag = "ZoneFieldWatch", .lane = 0.0f, .keys = "" },
                .sets = fields::kSets,
            };
            return app;
        }
    }
}

namespace zsdk::app
{
    const App &Get()
    {
        return zfieldwatch::Make();
    }
}
