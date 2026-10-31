#include <zsdk/shell.h>

#include "overlay/page.h"
#include "template.h"

ZSDK_ADDON("ZoneTemplate", TEMPLATE_DESCRIPTION)

namespace
{
    zsdk::Hooks Wire()
    {
        zsdk::Hooks h;
        h.refresh = &ztemplate::core::Refresh;
        h.prime = &ztemplate::core::Prime;
        h.step = &ztemplate::core::Step;
        h.hot = &ztemplate::core::Hot;
        h.publish = &ztemplate::core::Publish;
        h.turn = &ztemplate::core::Turn;
        h.order = &ztemplate::core::Order;
        h.orderName = &ztemplate::core::OrderName;
        h.complete = &ztemplate::core::Complete;
        h.frame = &ztemplate::frame::Frame;
        h.page = &ztemplate::page::Page;
        h.words = &ztemplate::page::Words;
        h.tone = &ztemplate::page::Tone;
        return h;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    static const zsdk::Hooks hooks = Wire();
    return zsdk::shell::Main(module, reason, reserved, hooks);
}
