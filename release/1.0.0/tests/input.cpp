#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include "../src/detail/runtime.h"

#include <string>

namespace
{
    using check::That;
    namespace input = zsdk::detail::input;

    struct Frame
    {
        uint64_t at;
        bool cursor;
        bool mods;
    };

    bool Run(std::initializer_list<Frame> frames, bool &anyMenu)
    {
        input::Glance g;
        bool last = false;
        anyMenu = false;
        for (const Frame &f : frames)
        {
            last = input::Steady(g, f.cursor, f.mods, f.at);
            anyMenu = anyMenu || last;
        }
        return last;
    }

    bool Menu(std::initializer_list<Frame> frames)
    {
        bool any = false;
        return Run(frames, any);
    }

    bool Ever(std::initializer_list<Frame> frames)
    {
        bool any = false;
        Run(frames, any);
        return any;
    }
}

int main()
{
    That(input::kSteadyMs == 400, "the OS cursor must stay 400 ms before it counts as a game screen");
    That(!Ever({ { 1000, true, false }, { 1010, true, false }, { 1020, true, false }, { 1030, false, false }, { 1600, false, false } }), "a 20 ms cursor blink is not a menu");
    That(Menu({ { 1000, true, false }, { 1250, true, false }, { 1500, true, false } }), "500 ms of a steady cursor is a menu");
    That(!Menu({ { 1000, true, false }, { 1399, true, false } }) && Menu({ { 1000, true, false }, { 1400, true, false } }), "the edge is exactly 400 ms");
    That(!Ever({ { 1000, true, true }, { 1500, true, true }, { 2000, true, true } }), "the cursor while Alt is held is not a menu");
    That(!Ever({ { 1000, true, true }, { 1800, true, true }, { 2600, true, true }, { 3300, true, true }, { 3350, true, false }, { 3600, true, false } }), "the 2.3 s Alt+G hold, and the moment after it, is not a menu");
    That(Menu({ { 1000, true, true }, { 3300, true, true }, { 3350, true, false }, { 3700, true, false } }), "400 ms after the keys are let go a still visible cursor is a menu again");
    That(!Menu({ { 1000, true, false }, { 1300, false, false }, { 1350, true, false }, { 1700, true, false } }), "a hidden frame restarts the count");
    That(Menu({ { 1000, true, false }, { 1300, false, false }, { 1350, true, false }, { 1750, true, false } }), "and the restarted count reaches 400 ms");
    That(!Ever({ { 5000, true, false }, { 4000, true, false }, { 4300, true, false } }), "a clock that goes back restarts the count");
    That(!Ever({ { 1000, false, true }, { 1200, true, false }, { 1500, true, false } }) && Menu({ { 1000, false, true }, { 1200, true, false }, { 1600, true, false } }),
         "a key held while the cursor is hidden still delays the menu by 400 ms");
    return check::Finish("input");
}
