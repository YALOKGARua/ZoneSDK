#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/engine_core.h"
#include "../src/detail/quit.h"
#include "../src/detail/runtime.h"
#include "support/fake.h"

#include <windows.h>

#include <tlhelp32.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using check::That;

    int wires = 0;
    bool answer = true;

    bool FakeWire()
    {
        ++wires;
        return answer;
    }

    std::mutex probeGate;
    int *volatile nowhere = nullptr;

    void Locked()
    {
        std::lock_guard<std::mutex> hold(probeGate);
        *nowhere = 1;
    }

    bool Caught()
    {
        __try
        {
            Locked();
            return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return true;
        }
    }

    bool Unwound()
    {
        if (!Caught()) return false;
        if (!probeGate.try_lock()) return false;
        probeGate.unlock();
        return true;
    }

    FARPROC Kernel(const char *name)
    {
        const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        return kernel ? GetProcAddress(kernel, name) : nullptr;
    }

    std::wstring Description(DWORD id)
    {
        using Read = HRESULT(WINAPI *)(HANDLE, PWSTR *);
        const FARPROC found = Kernel("GetThreadDescription");
        const HANDLE thread = found ? OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, id) : nullptr;
        if (!thread) return {};
        PWSTR text = nullptr;
        std::wstring out;
        if (SUCCEEDED(reinterpret_cast<Read>(reinterpret_cast<void *>(found))(thread, &text)) && text) out = text;
        if (text) LocalFree(text);
        CloseHandle(thread);
        return out;
    }

    int Named(const std::wstring &wanted)
    {
        const HANDLE shot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (shot == INVALID_HANDLE_VALUE) return 0;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        int found = 0;
        for (BOOL more = Thread32First(shot, &entry); more; more = Thread32Next(shot, &entry))
            if (entry.th32OwnerProcessID == GetCurrentProcessId() && Description(entry.th32ThreadID) == wanted) ++found;
        CloseHandle(shot);
        return found;
    }

    HWND Window()
    {
        WNDCLASSEXW kind{};
        kind.cbSize = sizeof(kind);
        kind.lpfnWndProc = DefWindowProcW;
        kind.hInstance = GetModuleHandleW(nullptr);
        kind.lpszClassName = L"UnrealWindow";
        RegisterClassExW(&kind);
        return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"UnrealWindow", L"zsdk thread suite", WS_POPUP | WS_VISIBLE, -32000, -32000, 64, 64, nullptr, nullptr, kind.hInstance, nullptr);
    }

    void Frames(int ms, bool pump)
    {
        for (int waited = 0; waited < ms; waited += 10)
        {
            zsdk::detail::thread::Present();
            MSG msg{};
            while (pump && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(10);
        }
    }

    template<class F> bool Within(int ms, F &&done)
    {
        for (int waited = 0; waited < ms; waited += 20)
        {
            if (done()) return true;
            Sleep(20);
        }
        return done();
    }

    std::string Refusal(const std::string &key, bool marked)
    {
        try
        {
            zsdk::detail::Gate(key, marked);
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

    constexpr UINT kCrashed = 70;
    constexpr UINT kTimedOut = 99;

    std::filesystem::path sheet;
    DWORD mainThread = 0;
    volatile uint8_t flag = 0;
    std::atomic<int> quits{ 0 };
    std::atomic<DWORD> quitThread{ 0 };
    std::atomic<bool> quitStopped{ false };
    std::atomic<bool> quitDusk{ false };
    std::atomic<bool> quitInSlow{ false };
    std::atomic<double> quitAt{ 0 };
    std::atomic<int> armed{ 0 };
    std::atomic<int> steps{ 0 };
    std::atomic<int> stepsLate{ 0 };
    std::atomic<bool> stepping{ false };
    std::atomic<int> overlaps{ 0 };
    std::atomic<double> stepEnd{ 0 };
    std::atomic<int> censuses{ 0 };
    std::atomic<int> censusLate{ 0 };
    std::atomic<int> beats{ 0 };
    std::atomic<int> beatsLate{ 0 };
    std::atomic<DWORD> beatThread{ 0 };
    std::atomic<bool> inSlow{ false };
    std::atomic<int> slowRuns{ 0 };
    std::atomic<bool> drawing{ false };
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

    LONG WINAPI Fell(EXCEPTION_POINTERS *info)
    {
        const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
        char line[128]{};
        _snprintf_s(line, sizeof(line), _TRUNCATE, "CRASH unhandled exception 0x%08lX on thread %lu", static_cast<unsigned long>(code), static_cast<unsigned long>(GetCurrentThreadId()));
        Put(line);
        TerminateProcess(GetCurrentProcess(), kCrashed);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    std::string Ms(double ms)
    {
        return std::to_string(static_cast<long long>(ms)) + " ms";
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

    void Quitting()
    {
        quitThread = GetCurrentThreadId();
        quitStopped = zsdk::thread::Stopping();
        quitDusk = zsdk::detail::engine::Dusked();
        quitInSlow = inSlow.load();
        if (stepping.load()) ++overlaps;
        quitAt = zsdk::Ms();
        ++quits;
    }

    void Nested()
    {
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
        const double from = zsdk::Ms();
        while (zsdk::Ms() - from < 50.0)
        {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(5);
        }
    }

    void Stepped(uint64_t)
    {
        stepping = true;
        ++steps;
        if (quits.load() > 0) ++stepsLate;
        const int mode = armed.exchange(0);
        if (mode == 1 || mode == 2) flag = 1;
        if (mode == 1) Sleep(300);
        if (mode == 2) Sleep(20);
        if (mode == 3)
        {
            Nested();
            Sleep(100);
        }
        stepEnd = zsdk::Ms();
        stepping = false;
    }

    bool Hot()
    {
        return true;
    }

    void Counted(const zsdk::census::Snapshot &)
    {
        ++censuses;
        if (quits.load() > 0) ++censusLate;
    }

    void Beat(uint64_t)
    {
        beatThread = GetCurrentThreadId();
        ++beats;
        if (quits.load() > 0) ++beatsLate;
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

    void Booted(const std::filesystem::path &folder)
    {
        Ok(zsdk::ini::Open(folder), "the data folder opens");
        zsdk::detail::Pretend(zsdk::exe::kSteamFile);
    }

    void Ended()
    {
        const uint64_t turned = zsdk::detail::thread::Turns();
        Sleep(300);
        Ok(zsdk::detail::thread::Turns() == turned, "Stop ends the census loop");
    }

    std::string Logged(const std::filesystem::path &folder)
    {
        return scan::Body(folder / "probe.log");
    }

    void Calmed(const std::filesystem::path &folder, const char *window)
    {
        const std::string body = scan::Body(folder / "probe.ini");
        const size_t at = body.find("\nwatch=");
        const size_t end = at == std::string::npos ? std::string::npos : body.find_first_of("\r\n", at + 7);
        const std::string watch = at == std::string::npos ? "?" : body.substr(at + 7, end == std::string::npos ? std::string::npos : end - at - 7);
        Ok(zsdk::ledger::Watched().empty() && watch.empty(), std::string("ledger::Calm closes the open window ") + window + " and writes it through");
    }

    void Lined(const std::filesystem::path &folder, const char *why)
    {
        zsdk::Flush();
        const std::string log = Logged(folder);
        const std::string line = std::string("game exit requested (") + why + ")";
        Ok(scan::Count(log, "game exit requested (") == 1 && scan::Count(log, line) == 1, "the exit sequence logs '" + line + "' exactly once");
    }

    int Worker(const std::filesystem::path &folder)
    {
        Booted(folder);
        fake::Game &game = *new fake::Game();
        Statics(game);
        zsdk::Hooks h;
        h.quit = &Quitting;
        h.census = &Counted;
        zsdk::hooks::Bind(h);
        Ok(zsdk::job::Every("probe.beat", 100, &Beat), "a 100 ms job is registered");
        zsdk::detail::thread::Cell(const_cast<const uint8_t *>(&flag));
        zsdk::detail::thread::Start();
        Ok(zsdk::detail::thread::Watching(), "the exit watcher reads the test flag cell");
        Ok(Within(10000, [] { return zsdk::thread::Booked() && censuses.load() > 0 && beats.load() > 2; }), "with no game thread hooked the census thread books the fake game, sweeps it and runs the job");
        Ok(zsdk::ledger::Watch("Probe.Exit"), "a crash window is open when the game asks to exit");
        Sleep(300);
        Ok(quits.load() == 0, "nothing happens while the flag is 0");
        const double flipped = zsdk::Ms();
        flag = 1;
        Ok(Within(2000, [] { return quits.load() > 0; }), "the flag goes from 0 to 1 and the census thread runs the exit sequence");
        const double late = quitAt.load() - flipped;
        Ok(late < 500.0, "the census loop sees the flag within a few 100 ms ticks, " + Ms(late));
        Ok(quitThread.load() != mainThread && quitThread.load() == beatThread.load(), "hooks.quit runs on the SDK census thread when only the worker polls");
        Ok(quitStopped.load() && quitDusk.load(), "engine calls are stopped before hooks.quit runs");
        Ok(Within(2000, [&folder] { return Logged(folder).find("game exit requested (GIsRequestingExit)") != std::string::npos; }), "the exit line reaches the disk with no flush from the test");
        Calmed(folder, "Probe.Exit");
        const std::string unflushed = Logged(folder);
        Ok(unflushed.find("safety: Probe.Exit no longer watched, the game quits") != std::string::npos && unflushed.find("safety: Probe.Exit no longer watched") < unflushed.find("game exit requested (GIsRequestingExit)"),
           "the sequence flushes the log once: the Calm line comes before the exit line and both are on disk");
        Ok(Refusal("Probe.Late", true) == "engine calls are stopped", "a later engine call is refused as stopped");
        Ok(zsdk::job::Stopping() && zsdk::thread::Stopping(), "job::Stopping() and thread::Stopping() are true after the exit flag");
        Sleep(1000);
        Ok(beatsLate.load() == 0, "no job runs after the exit flag, " + std::to_string(beatsLate.load()) + " runs");
        Ok(censusLate.load() == 0, "the census steps stop at the exit flag, hooks.census ran " + std::to_string(censusLate.load()) + " times after hooks.quit");
        const auto last = zsdk::census::Current();
        Ok(last && last->statics[0] != nullptr && last->statics[1] != nullptr, "the last census snapshot stays");
        const uint64_t turned = zsdk::detail::thread::Turns();
        Sleep(300);
        Ok(zsdk::detail::thread::Turns() > turned, "the census loop keeps turning without its steps");
        flag = 0;
        Sleep(250);
        flag = 1;
        Sleep(400);
        Ok(quits.load() == 1, "a second rise of the flag runs nothing, hooks.quit ran " + std::to_string(quits.load()) + " times");
        Lined(folder, "GIsRequestingExit");
        zsdk::detail::thread::Stop();
        Ended();
        return 0;
    }

    int Rejected(const std::filesystem::path &folder)
    {
        Booted(folder);
        zsdk::Hooks h;
        h.quit = &Quitting;
        zsdk::hooks::Bind(h);
        Ok(zsdk::job::Every("probe.beat", 100, &Beat), "a 100 ms job is registered");
        zsdk::detail::thread::Cell(const_cast<const uint8_t *>(&flag));
        zsdk::detail::thread::Start();
        Ok(Within(5000, [] { return zsdk::thread::Rejected(); }), "the test exe has no engine, its native bindings are rejected");
        const uint64_t turned = zsdk::detail::thread::Turns();
        Sleep(600);
        Ok(zsdk::detail::thread::Turns() >= turned + 3, "after 'native bindings rejected' the loop keeps ticking, " + std::to_string(zsdk::detail::thread::Turns() - turned) + " turns in 600 ms");
        flag = 1;
        Ok(Within(2000, [] { return quits.load() > 0; }), "on a rejected build the loop still polls the exit flag and runs the exit sequence");
        Ok(quitThread.load() != mainThread && quitThread.load() == beatThread.load() && quitStopped.load() && quitDusk.load(), "hooks.quit runs once on the census thread with engine calls stopped");
        Ok(Within(2000, [&folder] { return Logged(folder).find("game exit requested (GIsRequestingExit)") != std::string::npos; }), "the exit line reaches the disk with no flush from the test");
        Lined(folder, "GIsRequestingExit");
        zsdk::detail::thread::Stop();
        Ended();
        return 0;
    }

    int Pumped(const std::filesystem::path &folder)
    {
        Booted(folder);
        fake::Game &game = *new fake::Game();
        Statics(game);
        zsdk::detail::engine::Poise();
        zsdk::Hooks h;
        h.quit = &Quitting;
        h.step = &Stepped;
        h.hot = &Hot;
        zsdk::hooks::Bind(h);
        Ok(zsdk::job::Every("probe.slow", 100, &Slow), "a 700 ms job that never polls job::Stopping is registered");
        zsdk::detail::thread::Cell(const_cast<const uint8_t *>(&flag));
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        Ok(Until(10000, [] { return zsdk::thread::Hooked() && zsdk::thread::Booked() && steps.load() > 2 && slowRuns.load() > 0; }), "the pump runs hooks.step on the posed engine and the census thread runs the slow job");
        Ok(zsdk::ledger::Watch("Probe.Pump"), "a crash window is open when the game asks to exit");
        Ok(Until(3000, [] { return inSlow.load(); }), "the slow job holds the census thread");
        const double flipped = zsdk::Ms();
        flag = 1;
        Ok(Until(1500, [] { return quits.load() > 0; }), "the flag goes from 0 to 1 while the census thread is inside the job");
        const double late = quitAt.load() - flipped;
        Ok(quitThread.load() == mainThread && quitInSlow.load(), "the pump polls the flag on its own tick: hooks.quit runs on the game thread while the 700 ms job still holds the census thread");
        Ok(late < 400.0, "the exit does not wait for the job, " + Ms(late));
        Ok(overlaps.load() == 0, "hooks.quit does not overlap hooks.step");
        Calmed(folder, "Probe.Pump");
        Spin(1000);
        Ok(quits.load() == 1 && stepsLate.load() == 0, "hooks.quit ran once and no hooks.step came after it");
        Ok(Refusal("Probe.Late", true) == "engine calls are stopped", "a later engine call is refused as stopped");
        Lined(folder, "GIsRequestingExit");
        Unpaint();
        zsdk::detail::thread::Stop();
        Ended();
        DestroyWindow(window);
        return 0;
    }

    int Stepping(const std::filesystem::path &folder, int mode)
    {
        Booted(folder);
        fake::Game &game = *new fake::Game();
        Statics(game);
        zsdk::detail::engine::Poise();
        zsdk::Hooks h;
        h.quit = &Quitting;
        h.step = &Stepped;
        h.hot = &Hot;
        zsdk::hooks::Bind(h);
        zsdk::detail::thread::Cell(const_cast<const uint8_t *>(&flag));
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        Ok(Until(10000, [] { return zsdk::thread::Hooked() && steps.load() > 2; }), "the pump reaches hooks.step on the fake game with the engine posed ready");
        Ok(zsdk::ledger::Watch("Probe.Step"), "a crash window is open when the game asks to exit");
        armed = mode;
        Ok(Until(3000, [] { return quits.load() > 0; }), mode == 3 ? "a WM_QUIT pulled by a nested message loop inside hooks.step starts the exit" : "the flag goes from 0 to 1 inside hooks.step, in the pump scope");
        Spin(600);
        Ok(quits.load() == 1, "hooks.quit ran once, " + std::to_string(quits.load()) + " times");
        Ok(overlaps.load() == 0 && quitAt.load() >= stepEnd.load(), "hooks.quit never overlaps hooks.step: it starts after the step returned");
        Ok(stepsLate.load() == 0, "no hooks.step runs after hooks.quit");
        Ok(quitStopped.load() && quitDusk.load(), "engine calls are stopped before hooks.quit runs");
        zsdk::Flush();
        const std::string log = Logged(folder);
        const bool handed = log.find("exit sequence handed to the game thread") != std::string::npos;
        if (mode == 1)
            Ok(quitThread.load() == mainThread && handed, "the census thread saw the flag, the tick still ran after its 100 ms wait, so the game thread finished the exit sequence when hooks.step returned");
        else if (mode == 2)
            Ok(!handed, std::string("a 20 ms tick ends inside the 100 ms wait, the sequence ran on the ") + (quitThread.load() == mainThread ? "game thread" : "census thread") + " without a hand-off");
        else
            Ok(quitThread.load() == mainThread && !handed, "the nested WM_QUIT stops new pumps and engine calls at once, and the game thread runs hooks.quit when the tick returns");
        Calmed(folder, "Probe.Step");
        Ok(Refusal("Probe.Late", true) == "engine calls are stopped", "a later engine call is refused as stopped");
        Lined(folder, mode == 3 ? "WM_QUIT" : "GIsRequestingExit");
        Unpaint();
        zsdk::detail::thread::Stop();
        Ended();
        DestroyWindow(window);
        return 0;
    }

    int Off(const std::filesystem::path &folder)
    {
        Booted(folder);
        zsdk::Hooks h;
        h.quit = &Quitting;
        zsdk::hooks::Bind(h);
        const HWND window = Window();
        zsdk::detail::thread::Start();
        Paint();
        const auto own = zsdk::guard::Own();
        bool listed = own.size() == std::size(zsdk::detail::quit::kExits);
        bool off = true;
        for (size_t i = 0; i < std::size(zsdk::detail::quit::kExits); ++i)
        {
            listed = listed && i < own.size() && own[i] == zsdk::detail::quit::kExits[i].set;
            off = off && zsdk::guard::Of(*zsdk::detail::quit::kExits[i].set) == zsdk::guard::State::Off;
        }
        Ok(listed, "the SDK lists its exit sets, one per known build, among its own guard sets");
        Ok(!zsdk::detail::thread::Watching() && off, "no zsdk.exit set matches the test exe, so every exit set is off and the watcher reads nothing");
        Ok(Until(8000, [] { return zsdk::thread::Hooked() && zsdk::thread::Count().ticked > 0 && zsdk::thread::Rejected(); }), "the window thread is hooked and ticks, the bindings are rejected");
        const uint64_t turned = zsdk::detail::thread::Turns();
        Spin(600);
        Ok(zsdk::detail::thread::Turns() >= turned + 3, "the loop keeps ticking after 'native bindings rejected'");
        Ok(zsdk::ledger::Watch("Probe.Off"), "a crash window is open when the game quits");
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
        Spin(400);
        Ok(quits.load() == 1 && quitThread.load() == mainThread && quitStopped.load() && quitDusk.load(), "with the watcher off WM_QUIT still runs hooks.quit once, on the game thread, with engine calls stopped");
        Calmed(folder, "Probe.Off");
        zsdk::Flush();
        const std::string log = Logged(folder);
        Ok(log.find("exit watcher off: guard set zsdk.exit 0/2 match, image 0x") != std::string::npos && log.find("only WM_QUIT and the exit mark remain") != std::string::npos, "the log says the watcher is off after the byte mismatch");
        Lined(folder, "WM_QUIT");
        Unpaint();
        zsdk::detail::thread::Stop();
        Ended();
        DestroyWindow(window);
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
        if (name == L"exitworker") code = Worker(folder);
        else if (name == L"exitrejected") code = Rejected(folder);
        else if (name == L"exitpump") code = Pumped(folder);
        else if (name == L"exitstep") code = Stepping(folder, 1);
        else if (name == L"exitshort") code = Stepping(folder, 2);
        else if (name == L"exitnested") code = Stepping(folder, 3);
        else if (name == L"exitoff") code = Off(folder);
        zsdk::Flush();
        Put("end");
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
        return code;
    }

    std::filesystem::path nest;

    void Clean(const char *name, DWORD ms)
    {
        const std::filesystem::path folder = nest / name;
        std::error_code error;
        std::filesystem::remove_all(folder, error);
        std::filesystem::create_directories(folder, error);
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        const std::string label(name);
        std::wstring line = L"\"" + std::wstring(self) + L"\" child " + std::wstring(label.begin(), label.end()) + L" \"" + folder.wstring() + L"\"";
        STARTUPINFOW start{};
        start.cb = sizeof(start);
        PROCESS_INFORMATION proc{};
        const bool ran = CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &proc) != 0;
        bool done = false;
        DWORD code = 0;
        if (ran)
        {
            done = WaitForSingleObject(proc.hProcess, ms) == WAIT_OBJECT_0;
            if (!done)
            {
                TerminateProcess(proc.hProcess, kTimedOut);
                WaitForSingleObject(proc.hProcess, 5000);
            }
            GetExitCodeProcess(proc.hProcess, &code);
            CloseHandle(proc.hThread);
            CloseHandle(proc.hProcess);
        }
        std::vector<std::string> lines;
        const std::string body = scan::Body(folder / "sheet.txt");
        size_t from = 0;
        while (from < body.size())
        {
            const size_t end = body.find('\n', from);
            const std::string one = body.substr(from, end == std::string::npos ? std::string::npos : end - from);
            if (!one.empty()) lines.push_back(one);
            if (end == std::string::npos) break;
            from = end + 1;
        }
        That(ran && done, label + ": the child runs and ends within " + std::to_string(ms / 1000) + " s, no hang");
        std::string crash;
        for (const std::string &one : lines)
            if (one.rfind("CRASH ", 0) == 0) crash = one.substr(6);
        That(done && code == 0 && !lines.empty() && lines.back() == "end", label + ": the child reaches its end without a crash" + (crash.empty() ? std::string() : ", " + crash));
        for (const std::string &one : lines)
        {
            if (one.rfind("ok ", 0) == 0) That(true, label + ": " + one.substr(3));
            else if (one.rfind("FAIL ", 0) == 0) That(false, label + ": " + one.substr(5));
        }
    }

    void Watcher()
    {
        namespace quit = zsdk::detail::quit;
        That(std::size(quit::kExits) == std::size(zsdk::guard::kBuilds), "one zsdk.exit set per known build");
        for (size_t i = 0; i < std::size(quit::kExits); ++i)
        {
            const zsdk::guard::Set &set = *quit::kExits[i].set;
            const std::string id = set.build ? set.build->id : "-";
            That(set.count == 2 && set.build == zsdk::guard::kBuilds[i] && set.need == zsdk::guard::Need::ImageSize && std::string(set.name) == std::string(quit::kName) + "." + id,
                 "the exit set " + std::string(set.name) + ": two read-only guards pinned to build " + id + " by bytes and image size, named after the build");
            std::vector<int> store;
            That(zsdk::guard::Hex(set.guards[1].bytes, store) && store.size() >= 7 && store[0] == 0xC6 && store[1] == 0x05 && store[6] == 0x01,
                 "the second guard of " + std::string(set.name) + " is the one store 'mov byte ptr [GIsRequestingExit], 1'");
            bool listed = false;
            for (const zsdk::guard::Set *own : zsdk::guard::Own()) listed = listed || own == &set;
            That(listed, "guard::Own lists " + std::string(set.name) + ", so the generic guards suite compares it with a game exe");
        }
        That(quit::kList61bc1e03[0].rva == 0x41E14AEu && quit::kList61bc1e03[1].rva == 0x41E14F5u && quit::kFlag61bc1e03.at == 0x9FE3628u && quit::kTested61bc1e03.at == 0x9FE3628u,
             "61bc1e03: RequestEngineExit at 0x41E14AE, its store at +0x47 and its test at +0x18 both yield GIsRequestingExit 0x9FE3628");
        That(quit::kList2bd3f115[0].rva == 0x4186886u && quit::kList2bd3f115[1].rva == 0x41868CDu && quit::kFlag2bd3f115.at == 0x9FC9600u && quit::kTested2bd3f115.at == 0x9FC9600u,
             "2bd3f115: RequestEngineExit at 0x4186886, its store at +0x47 and its test at +0x18 both yield GIsRequestingExit 0x9FC9600");
        That(std::string(quit::kList2bd3f115[0].bytes) == "56 48 83 EC 20 48 89 CE B1 01 E8 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 05 72 ?? 80 3D 5B 2D E4 05 00" &&
                 std::string(quit::kList2bd3f115[1].bytes) == "C6 05 2C 2D E4 05 01 48 83 C4 20 5E C3",
             "2bd3f115: the anchor and the store bytes of RequestEngineExit");
        nest = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path()) / "exitwatch";
        Clean("exitworker", 30000);
        Clean("exitrejected", 20000);
        Clean("exitpump", 30000);
        Clean("exitstep", 30000);
        Clean("exitshort", 30000);
        Clean("exitnested", 30000);
        Clean("exitoff", 20000);
    }
}

void Unit()
{
    That(!zsdk::thread::OnGame() && !zsdk::detail::thread::InPump(), "a test thread is not the game thread");
    That(Refusal("A.B", true) == "refused off-thread engine call", "the choke point refuses calls off the game thread");
    {
        zsdk::detail::thread::Scope scope;
        That(zsdk::detail::thread::InPump(), "a pump scope is visible");
        That(Refusal("A.B", true) == "refused off-thread engine call", "a pump scope on a thread that is not the game thread is still refused");
    }
    That(!zsdk::detail::thread::InPump(), "the pump scope ends");
    std::string other;
    std::thread worker([&other] { other = Refusal("A.B", true); });
    worker.join();
    That(other == "refused off-thread engine call", "a worker thread is refused");
    alignas(16) uint8_t block[64]{};
    alignas(16) uint8_t fruit[64]{};
    That(zsdk::detail::engine::Ride(nullptr, nullptr, block, fruit) == zsdk::detail::engine::Rode::Refused, "the engine refuses a ride outside the pump before touching native code");
    That(!zsdk::detail::engine::Wire(), "wiring is impossible without a bound engine");
    That(zsdk::thread::State(false) == zsdk::Phase::Off, "the phase is off before start");
    That(!zsdk::thread::Stopping() && !zsdk::thread::Halted(), "nothing is stopping or halted");
    zsdk::thread::Ask(1);
    That(true, "asking without a game thread is harmless");
    const uint64_t step = GetTickCount64() - 50;
    const uint32_t epoch = zsdk::world::Hold().epoch;
    zsdk::detail::world::Jolt("test shift", step);
    That(zsdk::world::Hold().epoch == epoch + 1 && zsdk::world::Hold().since == step, "a world shift takes the now of the step that saw it, not a later tick read");
    That(zsdk::Age(step, zsdk::world::Hold().since) == 0 && zsdk::world::Hold().since <= step, "after a shift since is not later than the now passed to step, so the settle wait starts at 0");
    using Beat = zsdk::detail::thread::Beat;
    const auto lost = [](const Beat &beat, uint64_t now) { return zsdk::detail::thread::Lost(beat, now); };
    That(!lost(Beat{ .hooked = false, .pumpAt = 1000, .presentAt = 9000 }, 9000), "lost: never before the hook");
    That(!lost(Beat{ .hooked = true, .pumpAt = 0, .presentAt = 9000 }, 9000), "lost: never before the first tick");
    That(!lost(Beat{ .hooked = true, .pumpAt = 7100, .presentAt = 9000 }, 9000), "lost: 1.9 s without a tick is not lost yet");
    That(lost(Beat{ .hooked = true, .pumpAt = 7000, .presentAt = 9000 }, 9000), "lost: 2 s without a tick while presents run");
    That(!lost(Beat{ .hooked = true, .pumpAt = 1000, .presentAt = 8400 }, 9000), "lost: no present for 600 ms means the game does not draw, not lost");
    That(!lost(Beat{ .hooked = true, .pumpAt = 1000, .presentAt = 9000, .excusedAt = 7500 }, 9000), "lost: overlay, menu, pause or loading 1.5 s ago excuses the gap");
    That(lost(Beat{ .hooked = true, .pumpAt = 1000, .presentAt = 9000, .excusedAt = 7000 }, 9000), "lost: 2 s after the excuse ended with no tick");
    That(!lost(Beat{ .hooked = true, .pumpAt = 9500, .presentAt = 9000 }, 9000), "lost: a tick stamped after now is not a gap");
    const auto paused = [](uint64_t before, uint64_t now) { return zsdk::detail::thread::Gap(before, now); };
    That(paused(8400, 9000) && !paused(8600, 9000) && !paused(0, 9000) && !paused(9100, 9000), "a present after a 600 ms pause of the presenting thread excuses the pump gap, a 400 ms pause, the first present and a stamp after now do not");
    That(!lost(Beat{ .hooked = true, .pumpAt = 1000, .presentAt = 9000, .excusedAt = 9000 }, 9000), "lost: a present that follows a pause of the presenting thread itself, for example a starved PC or a loading screen, restarts the 2 s wait");
    That(zsdk::thread::LastPumpAt() == 0 && !zsdk::thread::PumpAlive() && zsdk::thread::Count().ticked == 0, "no pump before the core starts");
    That(zsdk::world::Snapshot().at == 0, "no snapshot is published before the first step");
    namespace wiring = zsdk::detail::thread;
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path folder = home / "wire";
    std::error_code code;
    std::filesystem::remove_all(folder, code);
    std::filesystem::create_directories(folder, code);
    That(zsdk::ini::WriteAtomic(folder / "probe.ini", "[safety]\r\nsession=00AA11BB22CC33DD\r\npending=engine.wire\r\n") && zsdk::ini::Open(folder), "a boot after a crash during wiring");
    That(zsdk::ledger::Blocked("engine.wire"), "the wiring key is disabled at boot");
    That(wiring::WireStep(&FakeWire) == wiring::Wired::Blocked && wires == 0, "a disabled wiring key stops the probe before any native code");
    That(wiring::WireBlocked() && zsdk::thread::State(false) == zsdk::Phase::Failed, "the phase shows the failure until Try again");
    zsdk::ledger::Retry();
    answer = true;
    That(zsdk::exe::Short() == "?", "the test host has no exe hash yet");
    That(wiring::WireStep(&FakeWire) == wiring::Wired::Done && wires == 1 && !wiring::WireBlocked(), "after Try again the probe runs once");
    const std::string body = scan::Body(folder / "probe.ini");
    That(body.find("pending=\r\n") != std::string::npos && body.find("engine.wire=") == std::string::npos, "a probe that returns clears its marker, and no proof is kept under the build ?");
    That(zsdk::ledger::Proven("engine.wire").empty(), "the wiring key is not proven without a known build");
    answer = false;
    That(wiring::WireStep(&FakeWire) == wiring::Wired::Failed && wires == 2, "a later probe in the same session runs without a new marker");
    That(scan::Body(folder / "probe.ini").find("pending=engine.wire") == std::string::npos, "no marker is left behind");
    zsdk::ini::Close();
    That(Unwound(), "an access violation under the present flags releases a held lock when an outer handler catches it");
    namespace engine = zsdk::detail::engine;
    const uint32_t books = engine::BookEpoch();
    const int probed = wires;
    uint64_t clock = GetTickCount64();
    {
        zsdk::detail::thread::Scope pump;
        That(engine::Stale() && wiring::Wiring(clock, &FakeWire) == wiring::Wired::Stale && wires == probed, "on the pump a stale function book skips the wire attempt");
        That(engine::Wanted() && engine::BookEpoch() == books, "the pump only asks the census thread for the book, it restocks nothing itself");
        That(wiring::Wiring(clock + 5000, &FakeWire) == wiring::Wired::Stale && wires == probed && engine::BookEpoch() == books, "while the book is wanted the pump keeps waiting and counts no attempt");
    }
    bool served = false;
    std::thread census([&served] { served = engine::Serve(); });
    census.join();
    That(served && !engine::Wanted() && !engine::Stale() && engine::BookEpoch() == books + 1, "the census thread walks the memory map and restocks the wanted book once");
    answer = false;
    wiring::Wired last = wiring::Wired::Idle;
    int failed = 0;
    {
        zsdk::detail::thread::Scope pump;
        for (int i = 0; i < 14; ++i)
        {
            clock += 31000;
            last = wiring::Wiring(clock, &FakeWire);
            if (last == wiring::Wired::Failed) ++failed;
        }
    }
    That(wires == probed + 10 && failed == 9, "wiring stops after 10 attempts, " + std::to_string(wires - probed) + " ran");
    That(last == wiring::Wired::Spent && zsdk::thread::State(true) == zsdk::Phase::Failed, "after the cap wiring is spent and the phase is Failed for the session");
    zsdk::detail::Pretend(zsdk::exe::kSteamFile);
    That(zsdk::exe::Steam(), "the thread suite runs as the Steam exe name, so the native bindings themselves are tried");
    zsdk::detail::thread::Start();
    That(zsdk::thread::Started(), "the core starts in the test process");
    That(Within(5000, [] { return zsdk::thread::Rejected(); }), "the test exe has no engine, its native bindings are rejected on the census thread");
    const uint64_t turned = wiring::Turns();
    Sleep(600);
    That(wiring::Turns() >= turned + 3, "after native bindings were rejected the census loop keeps ticking, every 100 ms, " + std::to_string(wiring::Turns() - turned) + " turns in 600 ms");
    That(zsdk::thread::State(false) == zsdk::Phase::Failed && !zsdk::thread::Bound() && !zsdk::thread::Booked(), "a rejected build shows Failed, nothing is bound or booked");
    if (Kernel("GetThreadDescription")) That(Named(L"ZoneSdkProbe census") == 1, "the SDK names its census thread '<add-on> census' through SetThreadDescription");
    else check::Warn("SKIPPED the thread name read-back: GetThreadDescription is missing on this Windows");
    const HWND window = Window();
    That(window != nullptr, "the suite owns a hidden UnrealWindow");
    zsdk::detail::thread::Present();
    That(zsdk::thread::Hooked() && zsdk::thread::OnGame(), "the core hooks the test thread that owns the UnrealWindow");
    Frames(400, true);
    const zsdk::thread::Counters live = zsdk::thread::Count();
    That(live.posted > 0 && live.seen > 0 && live.ticked > 0 && live.seen <= live.posted && live.ticked <= live.seen,
         "wake messages are posted, seen by the hook and tick the pump: posted " + std::to_string(live.posted) + ", seen " + std::to_string(live.seen) + ", ticked " + std::to_string(live.ticked));
    That(zsdk::thread::PumpAlive() && live.pumpAt == zsdk::thread::LastPumpAt() && zsdk::Age(GetTickCount64(), zsdk::thread::LastPumpAt()) < 500, "a ticking pump is alive and stamps its last tick");
    That(zsdk::thread::State(true) != zsdk::Phase::Lost, "a ticking pump is not lost");
    Frames(2400, false);
    const zsdk::thread::Counters gap = zsdk::thread::Count();
    That(zsdk::thread::State(true) == zsdk::Phase::Lost && !zsdk::thread::PumpAlive(), "presents run but the window thread pumps nothing for 2.4 s: Phase::Lost, ahead of the rejected build's Failed");
    That(gap.ticked == live.ticked && gap.seen == live.seen && gap.posted > live.posted, "during the gap wakes are still posted but none is seen and no tick runs");
    Frames(400, true);
    That(zsdk::thread::State(true) != zsdk::Phase::Lost && zsdk::thread::PumpAlive() && zsdk::thread::Count().ticked > gap.ticked && zsdk::thread::Count().seen > gap.seen, "the pump comes back and the phase leaves Lost");
    zsdk::detail::thread::Overlay(true);
    Frames(2400, false);
    That(zsdk::thread::State(true) != zsdk::Phase::Lost, "while the ReShade overlay is open a gap is not lost");
    zsdk::detail::thread::Overlay(false);
    Frames(300, true);
    DestroyWindow(window);
    zsdk::detail::thread::Stop();
    const uint64_t stopped = wiring::Turns();
    Sleep(300);
    That(wiring::Turns() == stopped && zsdk::thread::Stopping(), "Stop ends the census loop");
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 4 && std::wstring(argv[1]) == L"child") return Child(argv[2], std::filesystem::path(argv[3]));
    Unit();
    Watcher();
    return check::Finish("thread");
}
