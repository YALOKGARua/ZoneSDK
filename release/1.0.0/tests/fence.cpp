#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include <windows.h>

#include "../src/detail/lanes.h"
#include "../src/detail/runtime.h"

#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
    using check::That;
    namespace lanes = zsdk::detail::lanes;

    int *volatile nowhere = nullptr;
    volatile int sink = 0;
    volatile int depthCap = 0x7FFFFFFF;

    struct Mark
    {
        int *count;
        ~Mark()
        {
            ++*count;
        }
    };

    __declspec(noinline) int Deep(int depth)
    {
        volatile char pad[2048];
        pad[0] = static_cast<char>(depth);
        if (depth > depthCap) return pad[0];
        return Deep(depth + 1) + pad[0];
    }

    void Violate()
    {
        *nowhere = 1;
    }

    void Fences()
    {
        const zsdk::detail::Fended clean = zsdk::detail::Fend([] { sink = 1; });
        That(clean.code == 0 && !clean.thrown && clean.at == nullptr && clean.what.empty(), "a clean run reports nothing");

        const zsdk::detail::Fended av = zsdk::detail::Fend([] { Violate(); });
        That(av.code == EXCEPTION_ACCESS_VIOLATION && !av.thrown, "an access violation stops at the fence with its code");
        That(av.at != nullptr, "the fence keeps the faulting address");
        const std::string where = zsdk::detail::Where(av.at);
        That(where.find("+0x") != std::string::npos && where.find('\\') == std::string::npos && where.find(':') == std::string::npos, "the address is told as module+offset with no folder: " + where);

        const zsdk::detail::Fended thrown = zsdk::detail::Fend([] { throw std::runtime_error("boom"); });
        That(thrown.code == 0 && thrown.thrown && thrown.what == "boom", "a C++ exception passes the structured-exception filter and reaches the catch");

        const zsdk::detail::Fended odd = zsdk::detail::Fend([] { throw 7; });
        That(odd.code == 0 && odd.thrown && odd.what == "unknown exception", "a C++ exception of any type is caught as unknown");

        int inner = 0;
        const zsdk::detail::Fended caught = zsdk::detail::Fend([&inner] {
            try
            {
                throw std::logic_error("inside");
            }
            catch (const std::exception &)
            {
                ++inner;
            }
        });
        That(caught.code == 0 && !caught.thrown && inner == 1, "a C++ exception the work catches itself never reaches the fence");

        int unwound = 0;
        const zsdk::detail::Fended scoped = zsdk::detail::Fend([&unwound] {
            Mark mark{ &unwound };
            Violate();
        });
        That(scoped.code == EXCEPTION_ACCESS_VIOLATION && unwound == 1, "under /EHa the destructors between the fault and the fence run");

        const zsdk::detail::Fended divide = zsdk::detail::Fend([] { RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, nullptr); });
        That(divide.code == EXCEPTION_INT_DIVIDE_BY_ZERO, "any structured exception code is reported as raised");

        That(zsdk::detail::Where(nullptr) == "an unknown address", "no address reads as unknown");
    }

    void Overflows()
    {
        unsigned first = 0;
        unsigned second = 0;
        unsigned after = 1;
        std::thread runner([&] {
            first = zsdk::detail::Fend([] { sink = Deep(0); }).code;
            second = zsdk::detail::Fend([] { sink = Deep(0); }).code;
            after = zsdk::detail::Fend([] { sink = 2; }).code;
        });
        runner.join();
        That(first == EXCEPTION_STACK_OVERFLOW, "a stack overflow stops at the fence");
        That(second == EXCEPTION_STACK_OVERFLOW, "a second overflow on the same thread stops too, the guard page was restored");
        That(after == 0, "the thread runs clean work after two overflows");
    }

    void Lanes()
    {
        That(lanes::Count <= 32, "every lane fits in the mask");
        std::set<std::string> keys;
        for (uint32_t i = 0; i < lanes::Count; ++i)
        {
            const std::string key = lanes::kKey[i];
            That(key.rfind("present.", 0) == 0 && key.find(' ') == std::string::npos, "a lane key is present.<name>: " + key);
            keys.insert(key);
            That(lanes::kTag[i] && *lanes::kTag[i], "every lane has a log name");
        }
        That(keys.size() == lanes::Count, "lane keys are unique");
        That(lanes::Spread(lanes::Page) == lanes::Bit(lanes::Page) && lanes::Spread(lanes::PipelineBind) == lanes::Bit(lanes::PipelineBind), "a fault switches off its own lane");
        for (lanes::Lane lane : { lanes::Frame, lanes::Toasts })
            That(lanes::Spread(lane) == lanes::kShared, std::string("the frame and toasts lanes, which run add-on code over the same add-on state, go off together: ") + lanes::kTag[lane]);
        That(lanes::Spread(lanes::Data) == lanes::Bit(lanes::Data), "the data lane runs only SDK code under /EHa and goes off alone");
        for (lanes::Lane lane : { lanes::Boot, lanes::Core })
            That(lanes::Spread(lane) == lanes::kPark, std::string("a fault in boot or core stops the whole add-on: ") + lanes::kTag[lane]);
        That(!(lanes::kPark & (lanes::Bit(lanes::Device) | lanes::Bit(lanes::Runtime) | lanes::Bit(lanes::Save))) && (lanes::kPark & lanes::Bit(lanes::Page)),
             "a stopped add-on still frees its device objects and saves its ini, and its page shows the Try again fallback");
        That(lanes::Restore(lanes::Bit(lanes::Overlay)) == lanes::Bit(lanes::Overlay), "an earlier fault restores only its own lane");
        That(lanes::Restore(lanes::Bit(lanes::Frame)) == lanes::Bit(lanes::Frame), "a new session has no stuck lock, so a restored frame lane leaves data and toasts on");
        That((lanes::Restore(lanes::Bit(lanes::Core)) & lanes::kPark) == lanes::kPark, "an earlier boot or core fault parks the add-on until Try again");
        That(lanes::Off(0, 0) == 0, "Try again with nothing faulted this session clears every lane");
        That(lanes::Off(0, lanes::Bit(lanes::Frame)) == 0, "Try again brings a lane back that faulted this session");
        That(lanes::Off(lanes::Bit(lanes::Frame), lanes::Bit(lanes::Frame)) == lanes::kShared, "a lane still blocked keeps its shared-lock partners off");
        That(lanes::Off(0, lanes::Bit(lanes::Boot)) == lanes::kPark, "after a boot fault in this session the add-on stays off until the game restarts");
        That(lanes::Names(lanes::Bit(lanes::Frame) | lanes::Bit(lanes::Page)) == "frame, page" && lanes::Names(0).empty(), "lane names read as a list");
    }
}

int main()
{
    Fences();
    Overflows();
    Lanes();
    return check::Finish("fence");
}
