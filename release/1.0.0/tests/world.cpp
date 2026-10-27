#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/runtime.h"

#include <atomic>
#include <string>
#include <thread>

namespace
{
    using check::That;

    std::string NameOf(uint32_t epoch)
    {
        return "World_" + std::string(static_cast<size_t>(epoch % 61) + 20, 'x') + "_" + std::to_string(epoch);
    }
}

int main()
{
    That(!zsdk::thread::Hooked() && !zsdk::thread::OnGame(), "a test is neither hooked nor the game thread, so Hold() is the live grip here");
    That(&zsdk::world::Hold() == &zsdk::world::Hold(), "before hooking Hold() is the one live grip");
    std::atomic<bool> done{ false };
    std::atomic<int> torn{ 0 };
    std::atomic<int> reads{ 0 };
    constexpr uint32_t kWrites = 10000;
    std::thread game([&done] {
        for (uint32_t i = 1; i <= kWrites; ++i)
        {
            zsdk::world::Grip &w = zsdk::world::Hold();
            w.epoch = i;
            w.worldName = NameOf(i);
            w.seconds = static_cast<double>(i);
            w.stable = (i & 1u) != 0;
            zsdk::detail::world::Seen();
        }
        done = true;
    });
    std::thread reader([&done, &torn, &reads] {
        uint32_t last = 0;
        while (!done.load() || reads.load() < 100)
        {
            const zsdk::world::Grip &copy = zsdk::detail::world::Spare();
            ++reads;
            if (copy.epoch == 0) continue;
            if (copy.worldName != NameOf(copy.epoch) || copy.seconds != static_cast<double>(copy.epoch) || copy.stable != ((copy.epoch & 1u) != 0) || copy.epoch < last) ++torn;
            last = copy.epoch;
        }
    });
    game.join();
    reader.join();
    That(torn.load() == 0, "the off-thread copy never mixes two grips, " + std::to_string(reads.load()) + " reads");
    That(zsdk::detail::world::Spare().epoch == kWrites && zsdk::detail::world::Spare().worldName == NameOf(kWrites), "the copy is filled from the last published grip");
    zsdk::world::Grip &mine = zsdk::detail::world::Spare();
    That(&mine == &zsdk::detail::world::Spare() && &mine != &zsdk::world::Hold(), "each thread has its own copy, never the live grip");
    uint32_t other = 0;
    const zsdk::world::Grip *otherAt = nullptr;
    std::thread second([&other, &otherAt] {
        const zsdk::world::Grip &copy = zsdk::detail::world::Spare();
        other = copy.epoch;
        otherAt = &copy;
    });
    second.join();
    That(other == kWrites && otherAt != &mine, "another thread gets another copy with the same contents");
    const scan::Lexed lexed = scan::Lex(scan::Body(scan::SdkRoot() / "src" / "present" / "world.cpp"));
    const std::string &world = lexed.code;
    const size_t shift = world.find("void Shift(");
    const size_t end = world.find("Grip &Hold()");
    That(shift != std::string::npos && end != std::string::npos && world.find("detail::engine::ClearHosts()", shift) < end, "a world shift clears the engine host cache");
    const size_t hold = world.find("Grip &Hold()");
    That(hold != std::string::npos && world.find("detail::world::Spare()", hold) < world.find("Grip Snapshot()"), "Hold() off the game thread returns the copy");
    bool consoles = false;
    for (const std::string &literal : lexed.literals) consoles = consoles || literal == "wait.consoles";
    That(consoles && world.find("zsdk::census::Consoles()") != std::string::npos, "the world names a wait for the console hosts");
    return check::Finish("world");
}
