#include <zsdk/present.h>

#include "template.h"

#include <iterator>

namespace ztemplate
{
    namespace
    {
        const zsdk::App &Make()
        {
            static const zsdk::App app{
                .identity = zsdk::Identity{ .name = "ZoneTemplate", .tag = "ZoneTemplate", .description = TEMPLATE_DESCRIPTION, .version = kVersion, .folder = L"ZoneTemplate", .ini = L"template.ini", .log = L"template.log", .rolled = L"template.1.log", .registry = "template" },
                .policy = zsdk::Policy{},
                .specs = kSpecs,
                .world = zsdk::WorldVerbs{},
                .census = zsdk::CensusSpec{ .statics = common::kStatics, .staticCount = static_cast<int>(std::size(common::kStatics)) },
                .rows = Rows(),
                .schema = &Schema(),
                .row = zsdk::registry::Row{ .id = "template", .tag = "ZoneTemplate", .lane = 0.54f, .keys = "ALT+L" },
                .sets = {},
            };
            return app;
        }
    }
}

namespace zsdk::app
{
    const App &Get()
    {
        return ztemplate::Make();
    }
}
