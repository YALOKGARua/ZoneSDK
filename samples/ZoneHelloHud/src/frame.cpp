#include <zsdk/present.h>

#include "hellohud.h"

#include <atomic>

namespace zhellohud::frame
{
    namespace
    {
        uint64_t revision = 0;
        zsdk::keys::Chord toggle;
        bool on = false;
        std::atomic<bool> clear{ false };
    }

    void Frame(const zsdk::Input &input)
    {
        const uint64_t now = zsdk::ini::Revision();
        if (now != revision)
        {
            revision = now;
            const Settings s = zsdk::ini::Config<Settings>();
            toggle = s.toggle;
            on = s.enabled;
        }
        clear = input.Focused() && !input.Menu() && !input.Paused();
        if (!on || !input.Focused() || input.Overlaid() || input.Menu()) return;
        if (input.Fires(toggle)) zsdk::thread::Ask(static_cast<int>(Order::Toggle));
    }

    bool Clear()
    {
        return clear.load();
    }
}
