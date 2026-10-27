#include <zsdk/shell.h>

#include "fieldwatch.h"
#include "overlay/page.h"

ZSDK_ADDON("ZoneFieldWatch", FIELDWATCH_DESCRIPTION)

namespace
{
    zsdk::Hooks Wire()
    {
        zsdk::Hooks h;
        h.boot = &zfieldwatch::core::Boot;
        h.refresh = &zfieldwatch::core::Refresh;
        h.prime = &zfieldwatch::core::Prime;
        h.step = &zfieldwatch::core::Step;
        h.hot = &zfieldwatch::core::Hot;
        h.publish = &zfieldwatch::core::Publish;
        h.turn = &zfieldwatch::core::Turn;
        h.complete = &zfieldwatch::core::Complete;
        h.page = &zfieldwatch::page::Page;
        return h;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    static const zsdk::Hooks hooks = Wire();
    return zsdk::shell::Main(module, reason, reserved, hooks);
}
