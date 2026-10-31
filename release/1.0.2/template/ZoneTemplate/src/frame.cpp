#include <zsdk/present.h>

#include "template.h"

namespace ztemplate::frame
{
    namespace
    {
        uint64_t revision = 0;
        zsdk::keys::Chord locate;
        bool on = false;
    }

    void Frame(const zsdk::Input &input)
    {
        const uint64_t now = zsdk::ini::Revision();
        if (now != revision)
        {
            revision = now;
            const Settings s = zsdk::ini::Config<Settings>();
            locate = s.locate;
            on = s.enabled;
        }
        if (!on || !input.Focused() || input.Overlaid() || input.Menu()) return;
        if (input.Fires(locate)) zsdk::thread::Ask(static_cast<int>(Order::Locate));
    }
}
