#include <zsdk/shell.h>

#include "overlay/page.h"
#include "hellohud.h"

ZSDK_ADDON("ZoneHelloHud", HELLOHUD_DESCRIPTION)

namespace
{
    zsdk::Hooks Wire()
    {
        zsdk::Hooks h;
        h.refresh = &zhellohud::core::Refresh;
        h.prime = &zhellohud::core::Prime;
        h.step = &zhellohud::core::Step;
        h.hot = &zhellohud::core::Hot;
        h.publish = &zhellohud::core::Publish;
        h.turn = &zhellohud::core::Turn;
        h.order = &zhellohud::core::Order;
        h.orderName = &zhellohud::core::OrderName;
        h.complete = &zhellohud::core::Complete;
        h.frame = &zhellohud::frame::Frame;
        h.overlay = &zhellohud::corner::Draw;
        h.page = &zhellohud::page::Page;
        h.words = &zhellohud::page::Words;
        h.tone = &zhellohud::page::Tone;
        h.report = &zhellohud::core::Section;
        return h;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    static const zsdk::Hooks hooks = Wire();
    return zsdk::shell::Main(module, reason, reserved, hooks);
}
