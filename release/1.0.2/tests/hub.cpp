#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include "../src/detail/hub.h"

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using check::That;
    namespace hub = zsdk::detail::hub;

    constexpr int kCrowd = 64;

    std::atomic<int> pumps[3];
    std::atomic<int> quits[3];
    std::atomic<int> strangers{ 0 };
    std::atomic<bool> quitSeen{ false };
    UINT messages[4]{};

    template<int I> void Pump()
    {
        ++pumps[I];
    }

    template<int I> void Quit()
    {
        ++quits[I];
    }

    LRESULT CALLBACK Crowd(int code, WPARAM wparam, LPARAM lparam)
    {
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }

    bool Until(DWORD ms, const std::function<bool()> &done)
    {
        const uint64_t from = GetTickCount64();
        while (GetTickCount64() - from < ms)
        {
            if (done()) return true;
            Sleep(5);
        }
        return done();
    }

    struct Game
    {
        std::atomic<DWORD> id{ 0 };
        std::atomic<bool> stop{ false };
        std::thread thread;

        void Run()
        {
            thread = std::thread([this] {
                IsGUIThread(TRUE);
                MSG msg;
                PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);
                id = GetCurrentThreadId();
                while (!stop.load())
                {
                    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        if (msg.message == WM_QUIT)
                        {
                            quitSeen = true;
                            continue;
                        }
                        if (msg.message == messages[3]) ++strangers;
                        DispatchMessageW(&msg);
                    }
                    Sleep(1);
                }
            });
            while (!id.load()) Sleep(1);
        }

        void Halt()
        {
            stop = true;
            if (thread.joinable()) thread.join();
        }
    };

    int Sum(std::atomic<int> (&list)[3])
    {
        return list[0].load() + list[1].load() + list[2].load();
    }
}

int main()
{
    void *self = GetModuleHandleW(nullptr);
    messages[0] = RegisterWindowMessageW(L"ZoneSDK.HubSuite.A");
    messages[1] = RegisterWindowMessageW(L"ZoneSDK.HubSuite.B");
    messages[2] = RegisterWindowMessageW(L"ZoneSDK.HubSuite.C");
    messages[3] = RegisterWindowMessageW(L"ZoneSDK.HubSuite.Stranger");
    That(messages[0] >= 0xC000 && messages[3] >= 0xC000, "the suite registered its wake messages");

    hub::Deny(true);
    That(!hub::Allowed() && hub::Join(GetCurrentThreadId(), messages[0], &Pump<0>, &Quit<0>, self) == hub::Role::Off, "a denied hub never joins");
    hub::Deny(false);
    SetEnvironmentVariableW(L"ZSDK_SHARED_HOOK", L"0");
    That(!hub::Allowed(), "ZSDK_SHARED_HOOK=0 turns the shared hook off");
    SetEnvironmentVariableW(L"ZSDK_SHARED_HOOK", nullptr);
    That(hub::Allowed(), "without the switch the shared hook is allowed");

    Game game;
    game.Run();
    const DWORD tid = game.id.load();
    That(hub::Join(tid, messages[0], &Pump<0>, &Quit<0>, self) == hub::Role::Owner, "the first add-on installs the shared hook");
    That(hub::Join(tid, messages[1], &Pump<1>, &Quit<1>, self) == hub::Role::Guest, "the second add-on joins it as a guest");
    That(hub::Join(tid, messages[2], &Pump<2>, &Quit<2>, self) == hub::Role::Guest, "the third add-on joins it as a guest");
    That(hub::Members() == 3, "three add-ons share one hook");
    That(hub::Join(tid, 0x0400, &Pump<0>, &Quit<0>, self) == hub::Role::Off && hub::Join(tid, messages[0], nullptr, nullptr, self) == hub::Role::Off, "a wake id below the registered range or a missing pump is refused");

    for (int i = 0; i < 3; ++i) PostThreadMessageW(tid, messages[i], 0, 0);
    That(Until(2000, [] { return pumps[0].load() == 1 && pumps[1].load() == 1 && pumps[2].load() == 1; }), "each add-on is woken by its own message through the one hook");
    PostThreadMessageW(tid, messages[3], 0, 0);
    That(Until(2000, [] { return strangers.load() == 1; }) && Sum(pumps) == 3, "a message nobody registered passes on untouched");
    PostThreadMessageW(tid, messages[0], 0, 0);
    That(Until(2000, [] { return pumps[0].load() == 2; }), "a taken wake reaches the game loop as WM_NULL and does not count as a stranger");
    That(strangers.load() == 1, "the stranger count stays at one");

    hub::Leave(messages[1]);
    That(hub::Members() == 2, "an add-on that stops leaves the table");
    PostThreadMessageW(tid, messages[1], 0, 0);
    Sleep(200);
    That(pumps[1].load() == 1, "a left add-on is not woken any more");

    Game other;
    other.Run();
    That(hub::Join(other.id.load(), RegisterWindowMessageW(L"ZoneSDK.HubSuite.Other"), &Pump<1>, &Quit<1>, self) == hub::Role::Off, "while members live, a join for another thread keeps its own hook");
    other.Halt();

    std::vector<HHOOK> crowd;
    for (int i = 0; i < kCrowd; ++i)
        if (HHOOK h = SetWindowsHookExW(WH_GETMESSAGE, Crowd, nullptr, tid)) crowd.push_back(h);
    That(static_cast<int>(crowd.size()) == kCrowd, "64 more hooks join the chain after the shared one");
    const int before = pumps[0].load();
    PostThreadMessageW(tid, messages[0], 0, 0);
    Sleep(400);
    if (pumps[0].load() == before) That(true, "Windows never calls a hook that deep in the chain, the shared hook starves");
    else check::Warn("this Windows still calls a hook behind 64 others, the starving case is not shown");
    That(Until(8000, [self] { return hub::Rehook(self, GetTickCount64()); }), "after a quiet gap the shared hook moves to the head of the chain");
    That(hub::Moves() == 1, "the move is counted");
    That(!hub::Rehook(self, GetTickCount64()), "a second move right away is refused");
    const int first = pumps[0].load();
    const int third = pumps[2].load();
    PostThreadMessageW(tid, messages[0], 0, 0);
    PostThreadMessageW(tid, messages[2], 0, 0);
    That(Until(2000, [first, third] { return pumps[0].load() == first + 1 && pumps[2].load() == third + 1; }), "at the head again the shared hook wakes its add-ons past 64 other hooks");

    PostThreadMessageW(tid, WM_QUIT, 0, 0);
    That(Until(2000, [] { return quitSeen.load(); }) && quits[0].load() == 1 && quits[2].load() == 1 && quits[1].load() == 0, "WM_QUIT reaches every live add-on and skips the one that left");

    for (HHOOK h : crowd) UnhookWindowsHookEx(h);
    hub::Leave(messages[0]);
    hub::Leave(messages[2]);
    That(hub::Members() == 0, "the table is empty after every add-on left");

    Game moved;
    moved.Run();
    const UINT late = RegisterWindowMessageW(L"ZoneSDK.HubSuite.Late");
    That(hub::Join(moved.id.load(), late, &Pump<1>, &Quit<1>, self) == hub::Role::Owner, "with no members left the hub follows the game thread to a new thread");
    const int was = pumps[1].load();
    PostThreadMessageW(moved.id.load(), late, 0, 0);
    That(Until(2000, [was] { return pumps[1].load() == was + 1; }), "the moved hook wakes the member on the new thread");
    hub::Leave(late);
    moved.Halt();
    game.Halt();
    return check::Finish("hub");
}
