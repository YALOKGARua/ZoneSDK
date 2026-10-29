#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/runtime.h"

#include <windows.h>

#include <atomic>
#include <string>
#include <thread>

namespace
{
    using check::That;

    std::string NameOf(uint32_t epoch)
    {
        return "World_" + std::string(static_cast<size_t>(epoch % 53) + 24, 'y') + "_" + std::to_string(epoch);
    }

    HWND Window()
    {
        WNDCLASSEXW kind{};
        kind.cbSize = sizeof(kind);
        kind.lpfnWndProc = DefWindowProcW;
        kind.hInstance = GetModuleHandleW(nullptr);
        kind.lpszClassName = L"UnrealWindow";
        RegisterClassExW(&kind);
        return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"UnrealWindow", L"zsdk hold suite", WS_POPUP | WS_VISIBLE, -32000, -32000, 64, 64, nullptr, nullptr, kind.hInstance, nullptr);
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path logFile = home / "hold.log";
    zsdk::log::Open(logFile, home / "hold.1.log");
    zsdk::detail::thread::Start();
    const HWND window = Window();
    That(window != nullptr, "the suite owns a hidden UnrealWindow");
    zsdk::detail::thread::Present();
    That(zsdk::thread::Hooked() && zsdk::thread::OnGame(), "the suite thread is the hooked game thread");
    zsdk::world::Grip &live = zsdk::world::Hold();
    That(&live == &zsdk::world::Hold(), "on the game thread Hold() is the one live grip");
    const zsdk::world::Grip *first = nullptr;
    const zsdk::world::Grip *second = nullptr;
    std::thread probe([&first, &second] {
        first = &zsdk::world::Hold();
        second = &zsdk::world::Hold();
    });
    probe.join();
    That(first && first != &live && first == second, "after hooking another thread gets its own copy, the same thread_local object on each call, so a reference is valid only until that thread calls Hold() again");

    constexpr uint32_t kWrites = 10000;
    constexpr uint32_t kMostWrites = 4000000;
    std::atomic<bool> started{ false };
    std::atomic<bool> done{ false };
    std::atomic<int> torn{ 0 };
    std::atomic<int> reads{ 0 };
    std::atomic<int> overlap{ 0 };
    std::atomic<int> shared{ 0 };
    std::thread reader([&] {
        uint32_t last = 0;
        started = true;
        while (!done.load() || reads.load() < 100)
        {
            const bool writing = !done.load();
            const zsdk::world::Grip &copy = zsdk::world::Hold();
            ++reads;
            if (&copy == &live) ++shared;
            if (copy.epoch == 0) continue;
            if (writing) ++overlap;
            if (copy.worldName != NameOf(copy.epoch) || copy.seconds != static_cast<double>(copy.epoch) || copy.stable != ((copy.epoch & 1u) != 0) || copy.epoch < last) ++torn;
            last = copy.epoch;
        }
    });
    while (!started.load()) Sleep(1);
    uint32_t written = 0;
    for (uint32_t i = 1; i <= kWrites || (overlap.load() < 50 && i <= kMostWrites); ++i)
    {
        zsdk::world::Grip &w = zsdk::world::Hold();
        w.epoch = i;
        w.worldName = NameOf(i);
        w.seconds = static_cast<double>(i);
        w.stable = (i & 1u) != 0;
        zsdk::detail::world::Seen();
        written = i;
    }
    done = true;
    reader.join();
    That(shared.load() == 0, "the reader thread never got the live grip");
    That(overlap.load() >= 50, "the reads overlapped the rewrites, " + std::to_string(overlap.load()) + " of " + std::to_string(reads.load()) + " reads during " + std::to_string(written) + " rewrites");
    That(torn.load() == 0, "the real Hold() off the game thread never mixes two grips while the game thread rewrites it at least 10,000 times, " + std::to_string(torn.load()) + " torn");
    std::string tail;
    std::thread late([&tail] { tail = zsdk::world::Hold().worldName; });
    late.join();
    That(tail == NameOf(written), "the copy is filled from the last published grip");
    zsdk::Flush();
    const std::string log = scan::Body(logFile);
    That(log.find("world: the grip was read outside the game thread") != std::string::npos, "the off-thread read is logged as Rare");
    DestroyWindow(window);
    zsdk::detail::thread::Stop();
    return check::Finish("hold");
}
