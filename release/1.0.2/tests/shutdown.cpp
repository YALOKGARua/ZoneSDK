#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <windows.h>

#include "../src/detail/engine_core.h"
#include "../src/detail/runtime.h"
#include "support/fake.h"
#include "support/probe.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using check::That;

    constexpr UINT kCrashed = 70;
    constexpr UINT kTimedOut = 99;

    std::filesystem::path sheet;
    DWORD mainThread = 0;
    int *volatile nowhere = nullptr;
    volatile int depthCap = 1 << 30;

    std::atomic<int> quits{ 0 };
    std::atomic<DWORD> quitThread{ 0 };
    std::atomic<bool> quitStopped{ false };
    std::atomic<bool> quitDusk{ false };
    std::atomic<int> censuses{ 0 };
    std::atomic<int> censusLate{ 0 };
    std::atomic<int> beats{ 0 };
    std::atomic<int> beatsLate{ 0 };
    std::atomic<int> deeps{ 0 };
    std::atomic<int> taken{ 0 };
    std::atomic<int> lockFaults{ 0 };
    std::atomic<bool> inSlow{ false };
    std::atomic<int> slowRuns{ 0 };
    std::atomic<bool> drawing{ false };
    std::mutex jobGate;
    std::thread painter;

    void Put(const std::string &line)
    {
        const HANDLE file = CreateFileW(sheet.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        const std::string text = line + "\n";
        DWORD wrote = 0;
        WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &wrote, nullptr);
        CloseHandle(file);
    }

    void Ok(bool fine, const std::string &what)
    {
        Put((fine ? "ok " : "FAIL ") + what);
    }

    void Gap(bool fine, const std::string &id, const std::string &what)
    {
        Put(fine ? "ok " + what : "DEFECT " + id + " " + what);
    }

    LONG WINAPI Fell(EXCEPTION_POINTERS *info)
    {
        const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
        char line[128]{};
        _snprintf_s(line, sizeof(line), _TRUNCATE, "CRASH unhandled exception 0x%08lX on thread %lu%s", static_cast<unsigned long>(code), static_cast<unsigned long>(GetCurrentThreadId()),
                    GetCurrentThreadId() == mainThread ? " (the game thread)" : " (an SDK thread)");
        Put(line);
        TerminateProcess(GetCurrentProcess(), kCrashed);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    std::wstring Self()
    {
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        return self;
    }

    std::wstring Wide(const std::string &text)
    {
        return std::wstring(text.begin(), text.end());
    }

    std::string Ms(double ms)
    {
        return std::to_string(static_cast<long long>(ms)) + " ms";
    }

    HWND Window()
    {
        WNDCLASSEXW kind{};
        kind.cbSize = sizeof(kind);
        kind.lpfnWndProc = DefWindowProcW;
        kind.hInstance = GetModuleHandleW(nullptr);
        kind.lpszClassName = L"UnrealWindow";
        RegisterClassExW(&kind);
        return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"UnrealWindow", L"zsdk shutdown suite", WS_POPUP | WS_VISIBLE, -32000, -32000, 64, 64, nullptr, nullptr, kind.hInstance, nullptr);
    }

    void Paint()
    {
        drawing = true;
        painter = std::thread([] {
            while (drawing.load())
            {
                zsdk::detail::thread::Present();
                Sleep(10);
            }
        });
    }

    void Unpaint()
    {
        drawing = false;
        if (painter.joinable()) painter.join();
    }

    void Spin(int ms)
    {
        const uint64_t from = GetTickCount64();
        do
        {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(5);
        } while (zsdk::Age(GetTickCount64(), from) < static_cast<uint64_t>(ms));
    }

    template<class F> bool Until(int ms, F &&done)
    {
        for (int waited = 0; waited < ms; waited += 20)
        {
            if (done()) return true;
            Spin(20);
        }
        return done();
    }

    template<class F> bool Wait(int ms, F &&done)
    {
        for (int waited = 0; waited < ms; waited += 10)
        {
            if (done()) return true;
            Sleep(10);
        }
        return done();
    }

    std::string Refusal()
    {
        try
        {
            zsdk::detail::Gate("Probe.Late", true);
            return "passed";
        }
        catch (const zsdk::Refused &error)
        {
            return error.what();
        }
        catch (const std::exception &error)
        {
            return std::string("other: ") + error.what();
        }
    }

    std::string Line(const std::string &body, const std::string &key)
    {
        const std::string head = "\n" + key + "=";
        const size_t at = body.find(head);
        if (at == std::string::npos) return "?";
        const size_t from = at + head.size();
        const size_t end = body.find_first_of("\r\n", from);
        return body.substr(from, end == std::string::npos ? std::string::npos : end - from);
    }

    void Quitting()
    {
        quitThread = GetCurrentThreadId();
        quitStopped = zsdk::thread::Stopping();
        quitDusk = zsdk::detail::engine::Dusked();
        ++quits;
    }

    void QuitThrow()
    {
        ++quits;
        throw 42;
    }

    void QuitStd()
    {
        ++quits;
        throw std::runtime_error("shutdown suite quit throw");
    }

    void QuitFault()
    {
        ++quits;
        *nowhere = 1;
    }

    void Counted(const zsdk::census::Snapshot &)
    {
        ++censuses;
        if (quits.load() > 0) ++censusLate;
    }

    void CensusFault(const zsdk::census::Snapshot &)
    {
        ++censuses;
        *nowhere = 1;
    }

    void CensusThrow(const zsdk::census::Snapshot &)
    {
        ++censuses;
        throw 42;
    }

    void CensusStd(const zsdk::census::Snapshot &)
    {
        ++censuses;
        throw std::runtime_error("shutdown suite census throw");
    }

    bool CompleteFault(const zsdk::census::Snapshot &)
    {
        ++censuses;
        *nowhere = 1;
        return true;
    }

    void Beat(uint64_t)
    {
        ++beats;
        if (quits.load() > 0) ++beatsLate;
    }

    __declspec(noinline) int Dive(int depth)
    {
        volatile char pad[2048];
        pad[0] = static_cast<char>(depth);
        if (depth > depthCap) return pad[0];
        return Dive(depth + 1) + pad[0];
    }

    void Deep(uint64_t)
    {
        if (deeps++ == 0) Dive(0);
    }

    void Locker(uint64_t)
    {
        std::lock_guard<std::mutex> hold(jobGate);
        ++lockFaults;
        *nowhere = 1;
    }

    void Taker(uint64_t)
    {
        std::lock_guard<std::mutex> hold(jobGate);
        ++taken;
    }

    void Slow(uint64_t)
    {
        inSlow = true;
        ++slowRuns;
        Sleep(700);
        inSlow = false;
    }

    void Statics(fake::Game &game)
    {
        uint8_t *statics = game.Type("GameplayStatics", game.object);
        game.Make("Default__GameplayStatics", statics, nullptr);
        uint8_t *system = game.Type("KismetSystemLibrary", game.object);
        game.Make("Default__KismetSystemLibrary", system, nullptr);
    }

    bool Booted(const std::filesystem::path &folder)
    {
        const bool open = zsdk::ini::Open(folder);
        Ok(open, "the data folder opens");
        zsdk::detail::Pretend(zsdk::exe::kSteamFile);
        return open;
    }

    void Ended()
    {
        const uint64_t turns = zsdk::detail::thread::Turns();
        Sleep(300);
        Ok(zsdk::detail::thread::Turns() == turns, "Stop ends the census loop");
    }

    int Quit(const std::filesystem::path &folder)
    {
        Booted(folder);
        fake::Game &game = *new fake::Game();
        Statics(game);
        zsdk::Hooks h;
        h.quit = &Quitting;
        h.census = &Counted;
        zsdk::hooks::Bind(h);
        Ok(zsdk::job::Every("probe.beat", 100, &Beat), "a 100 ms job is registered");
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        Ok(Until(10000, [] { return zsdk::thread::Hooked() && zsdk::thread::Booked() && censuses.load() > 0 && zsdk::thread::Count().ticked > 0 && beats.load() > 2; }),
           "presents on a render thread hook the window thread, the census thread books the fake game and sweeps it, the pump ticks and the job runs");
        const auto before = zsdk::census::Current();
        Ok(before && before->statics[0] != nullptr && before->statics[1] != nullptr, "the census found both statics of the fake game");
        Ok(zsdk::ledger::Watch("Probe.Quit"), "a crash window is open when the game quits");
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
        Spin(300);
        Ok(quits.load() == 1, "two WM_QUIT messages run hooks.quit once, it ran " + std::to_string(quits.load()) + " times");
        Ok(quitThread.load() == mainThread, "hooks.quit runs on the game thread");
        Ok(quitStopped.load() && quitDusk.load(), "engine calls are stopped before hooks.quit runs");
        Ok(zsdk::ledger::Watched().empty() && Line(scan::Body(folder / "probe.ini"), "watch").empty(), "ledger::Calm closes the open window at WM_QUIT and writes it through");
        const std::string unflushed = scan::Body(folder / "probe.log");
        Ok(unflushed.find("no longer watched, the game quits") != std::string::npos && unflushed.find("game exit requested (WM_QUIT)") != std::string::npos,
           "the WM_QUIT path flushes the log once: the Calm line and the exit line are on disk right after WM_QUIT, before any later present or runtime drop flushes it");
        const zsdk::thread::Counters seen = zsdk::thread::Count();
        zsdk::thread::Ask(0);
        zsdk::thread::Wake();
        Spin(400);
        const zsdk::thread::Counters later = zsdk::thread::Count();
        Ok(later.posted == seen.posted && later.ticked == seen.ticked, "after WM_QUIT Ask and Wake post nothing and no pump tick runs");
        Ok(Refusal() == "engine calls are stopped", "an engine call after WM_QUIT is refused as stopped");
        Ok(zsdk::job::Stopping(), "job::Stopping() is true after WM_QUIT");
        Sleep(2600);
        Ok(zsdk::thread::State(true) != zsdk::Phase::Lost, "presents that go on 2.6 s after WM_QUIT without a tick are not Phase::Lost");
        Ok(beatsLate.load() == 0, "no job runs after WM_QUIT, " + std::to_string(beatsLate.load()) + " runs");
        Ok(censusLate.load() == 0, "the census steps stop at WM_QUIT: hooks.census ran " + std::to_string(censusLate.load()) + " times on the census thread after hooks.quit");
        const auto after = zsdk::census::Current();
        Ok(after && after->statics[0] != nullptr && after->statics[1] != nullptr, "the last census snapshot stays after WM_QUIT; a sweep cut short by thread::Stopping() is dropped, never published");
        const uint64_t turned = zsdk::detail::thread::Turns();
        Sleep(300);
        Ok(zsdk::detail::thread::Turns() > turned, "after WM_QUIT the census loop keeps turning without its steps, for the exit poll");
        zsdk::Flush();
        const std::string log = scan::Body(folder / "probe.log");
        Ok(log.find("safety: Probe.Quit no longer watched, the game quits") != std::string::npos, "the Calm line names the closed window");
        Ok(scan::Count(log, "game exit requested (WM_QUIT)") == 1, "the quit path logs 'game exit requested (WM_QUIT)' once");
        Unpaint();
        zsdk::detail::thread::Stop();
        Ended();
        DestroyWindow(window);
        return 0;
    }

    int Flag(const std::filesystem::path &folder)
    {
        Booted(folder);
        zsdk::Hooks h;
        h.quit = &Quitting;
        zsdk::hooks::Bind(h);
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        Ok(Until(8000, [] { return zsdk::thread::Hooked() && zsdk::thread::Count().ticked > 0; }), "the window thread is hooked and the pump ticks");
        Ok(zsdk::ledger::Watch("Probe.Flag"), "a crash window is open when the game quits");
        PostQuitMessage(0);
        Spin(300);
        Ok(quits.load() == 1, "a WM_QUIT made by PostQuitMessage reaches the WH_GETMESSAGE hook and runs hooks.quit once, it ran " + std::to_string(quits.load()) + " times");
        Ok(quits.load() != 1 || zsdk::ledger::Watched().empty(), "ledger::Calm runs with it");
        Unpaint();
        const zsdk::world::Grip *live = &zsdk::world::Hold();
        Ok(zsdk::thread::OnGame(), "before Stop the window thread is the game thread");
        double from = zsdk::Ms();
        zsdk::detail::thread::Stop();
        const double first = zsdk::Ms() - from;
        from = zsdk::Ms();
        zsdk::detail::thread::Stop();
        const double second = zsdk::Ms() - from;
        Ok(first < 500.0 && second < 50.0, "Stop returns soon and a second Stop is harmless, " + Ms(first) + " and " + Ms(second));
        const zsdk::world::Grip *after = &zsdk::world::Hold();
        zsdk::Flush();
        const bool reported = scan::Body(folder / "probe.log").find("the grip was read outside the game thread") != std::string::npos;
        Ok(after == live && !reported && !zsdk::thread::Hooked(),
           "after thread::Stop the former game thread still reads the live grip: Stop clears hooked with gameThread, so world::Hold() takes no copy under the shownGate lock, which hooks.stop at the process exit must not take");
        Ended();
        DestroyWindow(window);
        return 0;
    }

    int Bad(const std::filesystem::path &folder, void (*quit)())
    {
        Booted(folder);
        zsdk::Hooks h;
        h.quit = quit;
        zsdk::hooks::Bind(h);
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        Ok(Until(8000, [] { return zsdk::thread::Hooked() && zsdk::thread::Count().ticked > 0; }), "the window thread is hooked and the pump ticks");
        Ok(zsdk::ledger::Watch("Probe.Bad"), "a crash window is open when the game quits");
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
        Spin(300);
        Put("alive after WM_QUIT");
        Ok(quits.load() == 1, "hooks.quit ran once");
        Ok(zsdk::ledger::Watched().empty(), "ledger::Calm still closes the open window after hooks.quit failed");
        const std::string log = scan::Body(folder / "probe.log");
        Ok(scan::Count(log, "hooks.quit error: ") + scan::Count(log, "hooks.quit: structured exception ") == 1 && scan::Count(log, "game exit requested (WM_QUIT)") == 1,
           "the failed hooks.quit is one log line, then the exit line, both on disk at once");
        Unpaint();
        zsdk::detail::thread::Stop();
        Ended();
        DestroyWindow(window);
        return 0;
    }

    int Census(const std::filesystem::path &folder, const char *said, bool off, void (*hook)(const zsdk::census::Snapshot &), bool (*complete)(const zsdk::census::Snapshot &) = nullptr)
    {
        Booted(folder);
        fake::Game &game = *new fake::Game();
        Statics(game);
        zsdk::Hooks h;
        h.census = hook;
        h.complete = complete;
        zsdk::hooks::Bind(h);
        zsdk::detail::thread::Start();
        Ok(Wait(10000, [] { return censuses.load() > 0; }), "hooks.census runs on the census thread");
        const uint64_t turns = zsdk::detail::thread::Turns();
        Sleep(500);
        Put("alive after hooks.census failed");
        Ok(zsdk::detail::thread::Turns() > turns, "the census loop keeps ticking after hooks.census failed");
        zsdk::Flush();
        Ok(Wait(3000, [&folder, said] { return scan::Body(folder / "probe.log").find(said) != std::string::npos; }), std::string("the failure is logged: ") + said);
        Ok(zsdk::ledger::Blocked("census.hook") == off, off ? "a structured exception disables census.hook until Try again" : "a C++ exception leaves census.hook enabled");
        if (off)
        {
            const int seen = censuses.load();
            Sleep(2500);
            Ok(censuses.load() == seen, "after the structured exception neither hook runs again in this session, " + std::to_string(censuses.load() - seen) + " more runs");
            const auto snap = zsdk::census::Current();
            Ok(snap && snap->statics[0] != nullptr, "the census still sweeps and publishes without the add-on hooks");
        }
        zsdk::detail::thread::Stop();
        Ended();
        return 0;
    }

    int Stack(const std::filesystem::path &folder)
    {
        Booted(folder);
        Ok(zsdk::job::Every("probe.deep", 100, &Deep) && zsdk::job::Every("probe.beat", 100, &Beat), "a job that overflows its stack and a plain job are registered");
        zsdk::detail::thread::Start();
        Ok(Wait(5000, [] { return deeps.load() > 0 && zsdk::job::Off("probe.deep"); }), "the deep job ran and is off");
        const int runs = beats.load();
        Sleep(600);
        Put("alive after the stack overflow");
        Ok(beats.load() > runs, "the plain job keeps running on the census thread after the overflow");
        zsdk::Flush();
        Ok(scan::Body(folder / "probe.log").find("job probe.deep: structured exception 0xC00000FD") != std::string::npos, "the overflow is logged with its code");
        zsdk::detail::thread::Stop();
        Ended();
        return 0;
    }

    int Lock(const std::filesystem::path &folder)
    {
        Booted(folder);
        Ok(zsdk::job::Every("probe.locker", 100, &Locker) && zsdk::job::Every("probe.taker", 100, &Taker), "a job that faults while it holds an add-on mutex and a job that takes the same mutex are registered");
        zsdk::detail::thread::Start();
        Ok(Wait(5000, [] { return zsdk::job::Off("probe.locker"); }), "the faulting job is off");
        const int before = taken.load();
        Sleep(600);
        Put("alive after the fault");
        bool free = jobGate.try_lock();
        if (free) jobGate.unlock();
        Ok(lockFaults.load() == 1 && free, "the unwind of the fenced job released the add-on mutex it held");
        Ok(taken.load() > before, "the job that takes the same mutex keeps running, " + std::to_string(taken.load() - before) + " runs in 600 ms");
        zsdk::detail::thread::Stop();
        Ended();
        return 0;
    }

    int Long(const std::filesystem::path &folder)
    {
        Booted(folder);
        Ok(zsdk::job::Every("probe.slow", 100, &Slow) && zsdk::job::Every("probe.beat", 100, &Beat), "a 700 ms job that never polls job::Stopping and a plain job are registered");
        zsdk::detail::thread::Start();
        Ok(Wait(5000, [] { return slowRuns.load() > 0; }), "the slow job runs");
        uint64_t worst = 0;
        uint64_t last = zsdk::detail::thread::Turns();
        uint64_t at = GetTickCount64();
        const uint64_t from = at;
        while (zsdk::Age(GetTickCount64(), from) < 3000)
        {
            Sleep(5);
            const uint64_t now = GetTickCount64();
            const uint64_t turns = zsdk::detail::thread::Turns();
            if (turns == last) continue;
            worst = std::max(worst, zsdk::Age(now, at));
            last = turns;
            at = now;
        }
        Gap(worst < 400, "job-budget",
            "one job run cannot hold the SDK worker loop past its budget: the longest gap between two loop turns is " + std::to_string(worst) +
                " ms with a 700 ms job (tick 100 ms, job::kBudgetMs 50 ms); bind, the wanted book for wiring, census and the worker's own exit poll wait for it, the pump's exit poll does not");
        Ok(worst < zsdk::job::kStallMs + 300, "a job under job::kStallMs holds the SDK worker loop for one run at most, the longest gap is " + std::to_string(worst) + " ms");
        Ok(!zsdk::job::Off("probe.slow"), "a 700 ms job stays on, it is under job::kStallMs");
        const int runs = slowRuns.load();
        Ok(Wait(2000, [runs] { return slowRuns.load() > runs && inSlow.load(); }), "a fresh run of the slow job has just begun");
        const double start = zsdk::Ms();
        zsdk::detail::thread::Stop();
        const double stopMs = zsdk::Ms() - start;
        const bool still = inSlow.load();
        const int atStop = slowRuns.load();
        Ok(stopMs < 500.0, "Stop gives up its wait for the busy worker, " + Ms(stopMs));
        Ok(Wait(2000, [] { return !inSlow.load(); }), "the long run returns by itself");
        Sleep(400);
        Ok(slowRuns.load() == atStop && zsdk::job::Stopping(), std::string("after Stop no new job run starts; the run that was going when Stop returned ") + (still ? "finished by itself" : "had already ended") +
                                                                   ", the documented overlap with hooks.stop off DllMain, never on an unload (no census thread without the module pin)");
        Ended();
        return 0;
    }

    void Stall(uint64_t)
    {
        ++slowRuns;
        Sleep(1300);
    }

    int Stalled(const std::filesystem::path &folder)
    {
        Booted(folder);
        Ok(zsdk::job::Every("probe.stall", 100, &Stall) && zsdk::job::Every("probe.beat", 100, &Beat), "a 1300 ms job that never polls job::Stopping and a plain job are registered");
        zsdk::detail::thread::Start();
        Ok(Wait(5000, [] { return zsdk::job::Off("probe.stall"); }), "the job is off after its first run over job::kStallMs");
        const int beat = beats.load();
        Sleep(800);
        Ok(slowRuns.load() == 1, "it ran once, " + std::to_string(slowRuns.load()) + " runs");
        Ok(beats.load() > beat, "the plain job keeps running");
        Ok(!zsdk::ledger::Blocked("job.probe.stall"), "an over-long run switches the job off for this session only, its ledger key stays enabled");
        zsdk::Flush();
        Ok(scan::Body(folder / "probe.log").find("job probe.stall ran ") != std::string::npos && scan::Body(folder / "probe.log").find("ms one run may hold the SDK worker, the job is off for this session") != std::string::npos,
           "the over-long run is logged once");
        zsdk::detail::thread::Stop();
        Ended();
        return 0;
    }

    int Locks(const std::filesystem::path &folder)
    {
        Booted(folder);
        std::atomic<int> held{ 0 };
        std::atomic<bool> release{ false };
        std::thread holder([&held, &release] {
            zsdk::detail::world::Latch(true);
            zsdk::detail::census::Latch(true);
            held = 1;
            while (!release.load()) Sleep(5);
            zsdk::detail::census::Latch(false);
            zsdk::detail::world::Latch(false);
        });
        Ok(Wait(2000, [&held] { return held.load() == 1; }), "another thread holds the world snapshot lock and the census lock, as a thread killed inside a copy would");
        zsdk::detail::Exit(true);
        double from = zsdk::Ms();
        const zsdk::world::Grip copy = zsdk::world::Snapshot();
        const double snapMs = zsdk::Ms() - from;
        from = zsdk::Ms();
        const auto snap = zsdk::census::Current();
        const double currentMs = zsdk::Ms() - from;
        Ok(snapMs < 50.0 && copy.at == 0, "at the process exit world::Snapshot() only tries its lock and returns an empty grip, " + Ms(snapMs));
        Ok(currentMs < 50.0 && !snap, "at the process exit census::Current() only tries its lock and returns no snapshot, " + Ms(currentMs));
        from = zsdk::Ms();
        const zsdk::world::Grip *live = &zsdk::world::Hold();
        Ok(zsdk::Ms() - from < 50.0 && live != nullptr, "world::Hold() before the hook takes no lock at the exit");
        zsdk::detail::Exit(false);
        release = true;
        holder.join();
        Ok(zsdk::census::Current() == nullptr && zsdk::world::Snapshot().at == 0, "without the exit flag both locks are taken again once free");
        return 0;
    }

    int Leaving(const std::filesystem::path &folder)
    {
        Booted(folder);
        Ok(zsdk::job::Every("probe.slow", 100, &Slow), "a 700 ms job is registered");
        zsdk::detail::thread::Start();
        Ok(Wait(5000, [] { return inSlow.load(); }), "the slow job is inside a run");
        zsdk::detail::Exit(true);
        const double start = zsdk::Ms();
        zsdk::detail::thread::Stop();
        const double stopMs = zsdk::Ms() - start;
        Ok(stopMs < 100.0 && zsdk::job::Stopping(), "with the process-exit flag Stop does not wait for the busy worker, " + Ms(stopMs));
        return 0;
    }

    int Storm(const std::filesystem::path &folder)
    {
        Booted(folder);
        fake::Game &game = *new fake::Game();
        Statics(game);
        zsdk::Hooks h;
        h.census = &Counted;
        zsdk::hooks::Bind(h);
        Ok(zsdk::job::Every("probe.beat", 100, &Beat), "a 100 ms job is registered");
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        Ok(Until(10000, [] { return zsdk::thread::Hooked() && zsdk::thread::Booked() && zsdk::thread::Count().ticked > 0; }), "the core is hooked, booked and ticking");
        std::atomic<bool> go{ true };
        std::atomic<long long> calls{ 0 };
        std::vector<std::thread> crowd;
        for (int k = 0; k < 4; ++k)
            crowd.emplace_back([&go, &calls, k] {
                while (go.load())
                {
                    zsdk::thread::Ask(k);
                    zsdk::thread::Wake();
                    const zsdk::world::Grip copy = zsdk::world::Snapshot();
                    const zsdk::Phase phase = zsdk::thread::State(copy.stable);
                    const zsdk::thread::Counters counted = zsdk::thread::Count();
                    const bool off = zsdk::job::Off("probe.beat") || zsdk::ledger::Blocked("engine.wire");
                    const auto snap = zsdk::census::Current();
                    if (phase == zsdk::Phase::Count || counted.turns == ~0ull || off || (snap && snap->objects < 0)) ++calls;
                    ++calls;
                }
            });
        Spin(2500);
        go = false;
        for (std::thread &one : crowd) one.join();
        Ok(calls.load() > 1000, "four threads called Ask, Wake, Snapshot, State, Count, job::Off, ledger::Blocked and census::Current " + std::to_string(calls.load()) + " times while the pump ticked");
        const uint64_t ticked = zsdk::thread::Count().ticked;
        Ok(Until(3000, [ticked] { return zsdk::thread::Count().ticked > ticked; }), "the pump still ticks after the storm");
        Unpaint();
        const double start = zsdk::Ms();
        zsdk::detail::thread::Stop();
        Ok(zsdk::Ms() - start < 500.0, "Stop after the storm returns soon");
        Ended();
        DestroyWindow(window);
        return 0;
    }

    int Loader(const std::filesystem::path &folder)
    {
        Booted(folder);
        using Lock = LONG(NTAPI *)(ULONG, ULONG *, void **);
        using Unlock = LONG(NTAPI *)(ULONG, void *);
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        const auto lock = ntdll ? reinterpret_cast<Lock>(reinterpret_cast<void *>(GetProcAddress(ntdll, "LdrLockLoaderLock"))) : nullptr;
        const auto unlock = ntdll ? reinterpret_cast<Unlock>(reinterpret_cast<void *>(GetProcAddress(ntdll, "LdrUnlockLoaderLock"))) : nullptr;
        if (!lock || !unlock)
        {
            Put("SKIP the loader lock calls are missing in ntdll");
            return 0;
        }
        Ok(zsdk::job::Every("probe.beat", 100, &Beat), "a plain job is registered");
        zsdk::detail::thread::Start();
        Ok(Wait(3000, [] { return beats.load() > 2; }), "the census loop runs its job");
        void *cookie = nullptr;
        const bool held = lock(0, nullptr, &cookie) >= 0;
        const double start = zsdk::Ms();
        zsdk::detail::thread::Stop();
        const double stopMs = zsdk::Ms() - start;
        if (held) unlock(0, cookie);
        Ok(held, "the test holds the loader lock as DllMain does on an unload by FreeLibrary");
        Ok(stopMs < 100.0, "under the loader lock Stop waits only for the census loop to leave, never for the thread exit that needs that lock: it waited " + Ms(stopMs));
        Ended();
        return 0;
    }

    int Tick(const std::filesystem::path &folder)
    {
        Booted(folder);
        std::atomic<bool> go{ true };
        std::atomic<int> changes{ 0 };
        std::thread changer([&go, &changes] {
            probe::Settings s = zsdk::ini::Config<probe::Settings>();
            int n = 0;
            while (go.load())
            {
                s.radiusCm = 150.0 + static_cast<double>(++n % 100);
                zsdk::ini::Apply(s);
                ++changes;
            }
        });
        const uint64_t from = GetTickCount64();
        int ticks = 0;
        while (zsdk::Age(GetTickCount64(), from) < 3000)
        {
            zsdk::ini::Tick();
            ++ticks;
        }
        go = false;
        changer.join();
        zsdk::Flush();
        const size_t saves = scan::Count(scan::Body(folder / "probe.log"), "settings saved");
        Ok(changes.load() > 1000 && ticks > 1000, "settings change from another thread while ini::Tick runs, " + std::to_string(changes.load()) + " changes and " + std::to_string(ticks) + " ticks");
        Ok(saves == 0, "ini::Tick never saves while the settings keep changing (save delay 1 s), it saved " + std::to_string(saves) +
                           " times; Tick reads now before it locks and another thread may stamp a later changedAt, so the debounce takes Age(now, changedAt)");
        return 0;
    }

    int Child(const std::wstring &name, const std::filesystem::path &folder)
    {
        mainThread = GetCurrentThreadId();
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        SetUnhandledExceptionFilter(&Fell);
        sheet = folder / "sheet.txt";
        int code = 2;
        if (name == L"quit") code = Quit(folder);
        else if (name == L"flag") code = Flag(folder);
        else if (name == L"quitthrow") code = Bad(folder, &QuitThrow);
        else if (name == L"quitstd") code = Bad(folder, &QuitStd);
        else if (name == L"quitfault") code = Bad(folder, &QuitFault);
        else if (name == L"censusfault") code = Census(folder, "census: structured exception 0xC0000005 in hooks.census,", true, &CensusFault);
        else if (name == L"censusthrow") code = Census(folder, "census: hooks.census error: unknown exception", false, &CensusThrow);
        else if (name == L"censusstd") code = Census(folder, "census: hooks.census error: shutdown suite census throw", false, &CensusStd);
        else if (name == L"completefault") code = Census(folder, "census: structured exception 0xC0000005 in hooks.complete,", true, &Counted, &CompleteFault);
        else if (name == L"stack") code = Stack(folder);
        else if (name == L"lock") code = Lock(folder);
        else if (name == L"long") code = Long(folder);
        else if (name == L"stall") code = Stalled(folder);
        else if (name == L"locks") code = Locks(folder);
        else if (name == L"leaving") code = Leaving(folder);
        else if (name == L"tick") code = Tick(folder);
        else if (name == L"loader") code = Loader(folder);
        else if (name == L"storm") code = Storm(folder);
        zsdk::Flush();
        Put("end");
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
        return code;
    }

    struct Run
    {
        bool ran = false;
        bool done = false;
        DWORD code = 0;
        std::vector<std::string> lines;

        bool Ended() const
        {
            return !lines.empty() && lines.back() == "end";
        }

        bool Crashed() const
        {
            return done && (code != 0 || !Ended());
        }

        std::string Crash() const
        {
            for (const std::string &line : lines)
                if (line.rfind("CRASH ", 0) == 0) return line.substr(6);
            char text[64]{};
            _snprintf_s(text, sizeof(text), _TRUNCATE, "exit code 0x%08lX", static_cast<unsigned long>(code));
            return text;
        }
    };

    std::filesystem::path home;
    int known = 0;

    Run Start(const char *name, DWORD ms)
    {
        Run r;
        const std::filesystem::path folder = home / name;
        std::error_code error;
        std::filesystem::remove_all(folder, error);
        std::filesystem::create_directories(folder, error);
        std::wstring line = L"\"" + Self() + L"\" child " + Wide(name) + L" \"" + folder.wstring() + L"\"";
        STARTUPINFOW start{};
        start.cb = sizeof(start);
        PROCESS_INFORMATION proc{};
        r.ran = CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &proc) != 0;
        if (!r.ran) return r;
        r.done = WaitForSingleObject(proc.hProcess, ms) == WAIT_OBJECT_0;
        if (!r.done)
        {
            TerminateProcess(proc.hProcess, kTimedOut);
            WaitForSingleObject(proc.hProcess, 5000);
        }
        GetExitCodeProcess(proc.hProcess, &r.code);
        CloseHandle(proc.hThread);
        CloseHandle(proc.hProcess);
        const std::string body = scan::Body(folder / "sheet.txt");
        size_t from = 0;
        while (from < body.size())
        {
            const size_t end = body.find('\n', from);
            const std::string one = body.substr(from, end == std::string::npos ? std::string::npos : end - from);
            if (!one.empty()) r.lines.push_back(one);
            if (end == std::string::npos) break;
            from = end + 1;
        }
        return r;
    }

    void Known(const std::string &id, const std::string &what)
    {
        ++known;
        if (scan::Env("ZSDK_STRICT_DEFECTS") == "1") That(false, "known defect " + id + ": " + what);
        else check::Warn("known defect " + id + ": " + what);
    }

    void Take(const std::string &name, const Run &r)
    {
        for (const std::string &line : r.lines)
        {
            if (line.rfind("ok ", 0) == 0) That(true, name + ": " + line.substr(3));
            else if (line.rfind("FAIL ", 0) == 0) That(false, name + ": " + line.substr(5));
            else if (line.rfind("SKIP ", 0) == 0) check::Skip(name + ": " + line.substr(5));
            else if (line.rfind("DEFECT ", 0) == 0)
            {
                const size_t space = line.find(' ', 7);
                Known(line.substr(7, space == std::string::npos ? std::string::npos : space - 7), name + ": " + (space == std::string::npos ? std::string() : line.substr(space + 1)));
            }
        }
    }

    void Clean(const char *name, DWORD ms)
    {
        const Run r = Start(name, ms);
        That(r.ran && r.done, std::string(name) + ": the child runs and ends within " + std::to_string(ms / 1000) + " s, no hang");
        That(!r.Crashed(), std::string(name) + ": the child reaches its end without a crash" + (r.Crashed() ? ", " + r.Crash() : std::string()));
        Take(name, r);
    }
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 4 && std::wstring(argv[1]) == L"child") return Child(argv[2], std::filesystem::path(argv[3]));
    home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path()) / "shutdown";
    Clean("quit", 30000);
    Clean("flag", 20000);
    Clean("quitstd", 20000);
    Clean("quitthrow", 20000);
    Clean("quitfault", 20000);
    Clean("censusstd", 20000);
    Clean("censusthrow", 20000);
    Clean("censusfault", 20000);
    Clean("completefault", 20000);
    Clean("stack", 20000);
    Clean("lock", 20000);
    Clean("long", 20000);
    Clean("stall", 20000);
    Clean("locks", 20000);
    Clean("leaving", 20000);
    Clean("tick", 20000);
    Clean("loader", 20000);
    Clean("storm", 30000);
    if (known > 0 && scan::Env("ZSDK_STRICT_DEFECTS") != "1") std::printf("shutdown: %d known defects are warnings, ZSDK_STRICT_DEFECTS=1 makes them failures\n", known);
    return check::Finish("shutdown");
}
