#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/ledger_raw.h>
#include <zsdk/test/scan.h>

#include <windows.h>

#include "support/probe.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using check::That;
    namespace ini = zsdk::ini;
    namespace ledger = zsdk::ledger;
    using List = std::vector<std::string>;

    constexpr const char *kWin = "Probe.Window";
    constexpr const char *kImage = "Probe.Image";
    constexpr const char *kCall = "Probe.Call";

    std::unique_ptr<ledger::Window> lingering;

    bool Has(const List &list, const std::string &key)
    {
        return std::find(list.begin(), list.end(), key) != list.end();
    }

    std::filesystem::path Exit(const std::filesystem::path &folder)
    {
        return folder / "probe.exit";
    }

    std::filesystem::path Ini(const std::filesystem::path &folder)
    {
        return folder / "probe.ini";
    }

    bool Exists(const std::filesystem::path &file)
    {
        std::error_code code;
        return std::filesystem::exists(file, code);
    }

    void Put(const std::filesystem::path &file, const std::string &body)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << body;
    }

    bool Edit(const std::filesystem::path &file, const std::string &from, const std::string &to)
    {
        std::string body = scan::Body(file);
        const size_t at = body.find(from);
        if (at == std::string::npos) return false;
        body.replace(at, from.size(), to);
        Put(file, body);
        return true;
    }

    std::string Line(const std::filesystem::path &file, const std::string &key)
    {
        const std::string body = scan::Body(file);
        const std::string head = "\n" + key + "=";
        const size_t at = body.find(head);
        if (at == std::string::npos) return "?";
        const size_t from = at + head.size();
        const size_t end = body.find_first_of("\r\n", from);
        return body.substr(from, end == std::string::npos ? std::string::npos : end - from);
    }

    bool Logged(const std::filesystem::path &folder, const std::string &text)
    {
        zsdk::Flush();
        return scan::Body(folder / "probe.log").find(text) != std::string::npos;
    }

    std::string Sealed(const std::filesystem::path &folder)
    {
        return ledger::Mark(Line(Ini(folder), "session"), ledger::Seal(scan::Body(Ini(folder))));
    }

    void Plain(const std::filesystem::path &file)
    {
        SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL);
    }

    void Locked(const std::filesystem::path &file)
    {
        SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY);
    }

    std::wstring Wide(const std::string &text)
    {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring Env(const std::wstring &name)
    {
        wchar_t value[256]{};
        const DWORD n = GetEnvironmentVariableW(name.c_str(), value, 256);
        return n && n < 256 ? std::wstring(value, n) : std::wstring();
    }

    void SetEnv(const std::wstring &name, const std::string &value)
    {
        const std::wstring wide = Wide(value);
        SetEnvironmentVariableW(name.c_str(), value.empty() ? nullptr : wide.c_str());
    }

    std::wstring Self()
    {
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        return self;
    }

    struct Holder
    {
        PROCESS_INFORMATION proc{};
        HANDLE ready = nullptr;
        HANDLE done = nullptr;
        bool held = false;

        explicit Holder(const std::filesystem::path &file)
        {
            const std::wstring tag = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
            const std::wstring readyName = L"Local\\zsdk-exit-ready-" + tag;
            const std::wstring doneName = L"Local\\zsdk-exit-done-" + tag;
            ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
            done = CreateEventW(nullptr, TRUE, FALSE, doneName.c_str());
            std::wstring line = L"\"" + Self() + L"\" hold \"" + file.wstring() + L"\" " + readyName + L" " + doneName;
            STARTUPINFOW start{};
            start.cb = sizeof(start);
            if (!ready || !done || !CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &proc)) return;
            held = WaitForSingleObject(ready, 30000) == WAIT_OBJECT_0;
        }

        ~Holder()
        {
            Release();
            if (ready) CloseHandle(ready);
            if (done) CloseHandle(done);
        }

        void Release()
        {
            if (done) SetEvent(done);
            if (proc.hProcess)
            {
                WaitForSingleObject(proc.hProcess, 30000);
                CloseHandle(proc.hThread);
                CloseHandle(proc.hProcess);
                proc = PROCESS_INFORMATION{};
            }
        }
    };

    int Hold(const std::filesystem::path &file, const std::wstring &readyName, const std::wstring &doneName)
    {
        const HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyName.c_str());
        const HANDLE done = OpenEventW(SYNCHRONIZE, FALSE, doneName.c_str());
        if (!ready || !done) return 5;
        const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return 6;
        SetEvent(ready);
        WaitForSingleObject(done, 60000);
        CloseHandle(handle);
        CloseHandle(ready);
        CloseHandle(done);
        return 0;
    }

    void Pure()
    {
        That(ledger::Session("0A1b2C") && !ledger::Session("") && !ledger::Session("XYZ") && !ledger::Session("12 34") && !ledger::Session(std::string(ledger::kSessionMost + 1, 'A')),
             "a session id is short hex");
        const std::string body = "[safety]\r\nsession=ABC\r\npending=\r\ndisabled=\r\nwatch=Probe.Window\r\n";
        const std::string seal = ledger::Seal(body);
        That(seal.size() == 16 && ledger::Session(seal), "the seal is 16 hex digits");
        That(ledger::Seal("") == "14650FB0739D0383" && ledger::Seal("a") == "44BD8AD473CD9906", "the seal is the FNV-1a 64 walk with the start value of the 0.1.2 exit patch, so marks written by that fork stay valid");
        That(ledger::Seal(body) == seal && ledger::Seal(body + ";") != seal && ledger::Seal("[safety]\r\npending=\r\ndisabled=\r\nwatch=Probe.Window\r\nsession=ABC\r\n") != seal && ledger::Seal("") != seal,
             "the seal covers every byte of the ini, the order of the lines too");
        That(ledger::Mark("ABC", seal) == "clean ABC " + seal + "\r\n", "the mark is one short line");
        That(ledger::Mark("", seal).empty() && ledger::Mark("not hex", seal).empty() && ledger::Mark("ABC", "").empty(), "no mark without a session and a seal");
        const std::string mark = ledger::Mark("ABC", seal);
        That(ledger::Marked(mark, "ABC", seal) && ledger::Marked("clean ABC " + seal, "ABC", seal) && ledger::Marked(mark + std::string(4, '\0'), "ABC", seal), "the exact mark matches, a padded tail too");
        That(!ledger::Marked(mark, "ABD", seal) && !ledger::Marked(mark, "", seal) && !ledger::Marked(mark, "ABC", ledger::Seal(body + " ")), "a mark of another run or another file does not match");
        That(!ledger::Marked("clean ABC", "ABC", seal) && !ledger::Marked("clean ABC " + seal.substr(0, 8), "ABC", seal) && !ledger::Marked(" " + mark, "ABC", seal) &&
                 !ledger::Marked("dirty ABC " + seal, "ABC", seal) && !ledger::Marked("", "ABC", seal) && !ledger::Marked(std::string(ledger::kMarkMost + 1, 'c'), "ABC", seal),
             "a cut, shifted, odd or huge mark does not match");
        const List watch = { kWin, kImage };
        ledger::Leftover l = ledger::Judge(mark, "ABC", seal, "", watch);
        That(l.clean && !l.older && l.closed == watch && l.fallen.empty() && l.cleared.empty(), "clean exit marked: the windows left open are closed, nothing is turned off");
        l = ledger::Judge("", "ABC", seal, "", watch);
        That(!l.clean && l.closed.empty() && l.fallen == watch, "clean exit not marked: every open window counts as a crash");
        l = ledger::Judge(ledger::Mark("ABB", seal), "ABC", seal, "", watch);
        That(!l.clean && l.fallen == watch, "a mark left by an older run counts as a crash");
        l = ledger::Judge(mark.substr(0, 12), "ABC", seal, "", watch);
        That(!l.clean && l.fallen == watch, "a cut mark counts as a crash");
        l = ledger::Judge(mark, "ABC", ledger::Seal(body + "x"), "", watch);
        That(!l.clean && l.fallen == watch, "an ini changed after the mark counts as a crash");
        l = ledger::Judge(mark, "ABC", "", "", watch);
        That(!l.clean && !l.older && l.fallen == watch, "no ini read, no clean exit");
        l = ledger::Judge(mark, "", seal, kCall, watch);
        That(!l.clean && l.older && l.fallen.empty() && l.closed.empty() && l.cleared == List{ kCall, kWin, kImage }, "an ini without a session was written by an older ZoneSDK: not judged, its markers are cleared");
        l = ledger::Judge("", "", seal, "", {});
        That(l.older && l.cleared.empty() && l.fallen.empty(), "an older ini with nothing armed clears nothing");
        l = ledger::Judge("", "", "", kCall, watch);
        That(!l.older && l.fallen == List{ kCall, kWin, kImage }, "without an ini on disk there is nothing older to excuse");
        l = ledger::Judge(mark, "not hex", seal, "", watch);
        That(!l.older && !l.clean && l.fallen == watch, "a broken session line is judged, it counts as a crash");
        l = ledger::Judge(mark, "ABC", seal, kCall, watch);
        That(l.clean && l.closed == watch && l.fallen == List{ kCall }, "a first call still pending at the mark stays a crash, the windows are closed");
        const List both = { kCall, kWin };
        l = ledger::Judge(mark, "ABC", seal, kCall, both);
        That(l.clean && l.closed == List{ kWin } && l.fallen == List{ kCall }, "a key both pending and watched is turned off, not closed");
        l = ledger::Judge(mark, "ABC", seal, "", {});
        That(l.clean && l.closed.empty() && l.fallen.empty(), "a clean exit with nothing open changes nothing");
        l = ledger::Judge("", "ABC", seal, kCall, { kWin, kWin });
        That(l.fallen == List{ kCall, kWin }, "the pending key goes first and no key is listed twice");
        That(ledger::Ended(true, 0, true), "the process exit on the game thread after the last swap chain went is a clean end");
        That(!ledger::Ended(false, 0, true) && !ledger::Ended(false, 0, false), "exit() from another thread (the crash handler of a worker thread) is not a clean end");
        That(!ledger::Ended(true, 1, true) && !ledger::Ended(true, 2, false), "an exit while a swap chain still draws frames is not a clean end");
        That(!ledger::Ended(true, 0, false), "an exit before the swap chain was ever let go is not a clean end");
        That(!ledger::Ended(true, -1, false) && ledger::Ended(true, -1, true), "a count below zero is treated as no swap chain");
    }

    void Text()
    {
        probe::Settings s;
        ini::Kept k;
        k.session = "00AA11BB";
        k.watch = { kWin };
        const std::string text = ini::Text(s, k);
        That(text.find("[safety]\r\nsession=00AA11BB\r\npending=\r\ndisabled=\r\nwatch=Probe.Window\r\n") != std::string::npos, "the ini keeps the session of the run first in [safety]");
        probe::Settings back;
        ini::Kept read;
        ini::Parse(text, back, &read);
        That(read.session == "00AA11BB" && read.watch == List{ kWin } && read.unknown.empty(), "the session reads back as its own key");
        ini::Kept late;
        ini::Parse("[safety]\r\npending=\r\nwatch=\r\nsession=00CC\r\n", back, &late);
        That(late.session == "00CC" && late.unknown.empty(), "a session line after the other keys, as an older ZoneSDK keeps it, still reads back");
    }

    void Unarmed(const std::filesystem::path &home)
    {
        ledger::raw::Leave();
        That(!Exists(Exit(home / "z")) && !Exists(home / "probe.exit"), "before the data folder opens the exit writes no mark");
    }

    bool Hex(const std::string &text)
    {
        return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); });
    }

    void Clean(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "clean";
        That(ini::Open(folder), "a fresh data folder opens");
        const std::string first = ledger::Current();
        That(first.size() == 32 && Hex(first), "the session id is 32 upper-case hex digits (pid, time, performance counter)");
        That(ledger::Session(first) && Line(Ini(folder), "session") == first, "the run has a session id on disk before any window opens");
        That(!ledger::CleanStart() && ledger::Closed().empty() && ledger::Last().empty() && !ledger::Stuck(), "a first start has no clean exit behind it");
        That(ini::ExitPath() == Exit(folder), "the mark lives next to the ini as <ini name>.exit");
        That(ledger::Watch(kWin) && ledger::Watch(kImage) && Has(ledger::Watched(), kWin), "two crash windows open");
        That(!Exists(Exit(folder)), "no mark while the game runs");
        ledger::raw::Leave();
        That(Exists(Exit(folder)), "the clean exit writes the mark");
        That(scan::Body(Exit(folder)) == Sealed(folder), "the mark names this run and seals the exact bytes of the ini");
        That(Line(Ini(folder), "watch") == "Probe.Window,Probe.Image", "the windows stay open in the ini, as when the game quits without WM_QUIT");
        ledger::raw::Leave();
        That(scan::Body(Exit(folder)) == Sealed(folder), "a second exit call changes nothing");
        That(ini::Open(folder), "the next start opens the same folder");
        const std::string second = ledger::Current();
        That(ledger::CleanStart() && ledger::Last() == first, "the next start sees the clean exit of the run before it");
        That(ledger::Closed() == List{ kWin, kImage }, "the windows left open are closed, not turned off");
        That(ledger::Disabled().empty() && ledger::Booted().empty() && !ledger::Blocked(kWin) && !ledger::Blocked(kImage), "nothing is disabled after a clean exit");
        That(!Exists(Exit(folder)) && !ledger::Stuck(), "the mark is used once and removed");
        That(ledger::Session(second) && second != first && Line(Ini(folder), "session") == second, "the next run has its own session id");
        That(Line(Ini(folder), "watch").empty() && Line(Ini(folder), "disabled").empty(), "the ini has no open and no disabled windows");
        That(Logged(folder, "ended cleanly (exit mark found), the crash windows it left open are closed: Probe.Window,Probe.Image"), "the log says the last run ended cleanly");
    }

    void Frozen(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "frozen";
        That(ini::Open(folder), "a fresh data folder opens");
        auto window = std::make_unique<ledger::Window>("Probe.Scoped");
        That(window->Armed() && ledger::Watch(kWin), "a scoped window and a plain window are open and armed");
        That(Line(Ini(folder), "watch") == "Probe.Scoped,Probe.Window", "both windows are on disk");
        ledger::raw::Leave();
        const std::string body = scan::Body(Ini(folder));
        const std::string mark = scan::Body(Exit(folder));
        window.reset();
        That(scan::Body(Ini(folder)) == body, "an armed window destroyed after the mark (a static destructor at the exit) does not write the ini");
        That(!ledger::Watch(kImage) && !ledger::Unwatch(kWin, std::string()) && !ledger::Calm(), "after the mark the crash windows do not change");
        That(!ledger::raw::Arm(kCall) && !ledger::raw::Prove(kCall, "abc") && !ledger::raw::Settle() && !ledger::raw::Disable(kCall), "after the mark the first calls do not change");
        ledger::Retry();
        That(!ini::Store("probe", {}) && !ini::Put("memory", "x", "1") && !ini::Save(), "after the mark nothing else is written either");
        const ledger::Window late("Probe.Late");
        That(late.Refused() && !late.Armed(), "a window asked for after the mark is refused");
        That(scan::Body(Ini(folder)) == body && scan::Body(Exit(folder)) == mark, "the ini and the mark are byte for byte as at the mark");
        That(ini::Open(folder), "the next start opens");
        That(ledger::CleanStart() && ledger::Closed() == List{ "Probe.Scoped", kWin } && ledger::Disabled().empty(), "the clean exit stays clean: both windows are closed, nothing is off");
        That(!Logged(folder, "does not match"), "the log has no stale mark");
        That(ledger::Watch(kImage) && ledger::Unwatch(kImage, std::string()), "a new start writes again");
    }

    void Freeze(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "freeze";
        That(ini::Open(folder), "a fresh data folder opens");
        That(ledger::Watch(kWin), "a window is open");
        const std::string body = scan::Body(Ini(folder));
        ledger::raw::Freeze();
        That(!Exists(Exit(folder)), "a process exit that is not a clean end freezes the ini without a mark");
        That(!ledger::Unwatch(kWin, std::string()) && !ledger::Calm() && !ini::Save() && scan::Body(Ini(folder)) == body, "after the freeze nothing is written");
        That(ini::Open(folder), "the next start opens");
        That(!ledger::CleanStart() && ledger::Blocked(kWin), "an exit without a mark counts as a crash");
        ledger::Retry();
    }

    void Reload(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "reload";
        That(ini::Open(folder), "game 1 opens a fresh data folder");
        const std::wstring name = ledger::raw::ReloadName();
        That(name.rfind(L"ZSDK_RELOAD_", 0) == 0 && name.size() > 12, "the reload memo is named after the add-on");
        SetEnv(name, {});
        const std::string game1 = ledger::Current();
        ledger::raw::Leave();
        That(ini::Open(folder), "game 2, the first load of the add-on");
        const std::string load1 = ledger::Current();
        That(ledger::CleanStart() && ledger::Last() == game1, "the first load sees the clean exit of game 1");
        ledger::Calm();
        ini::Close();
        ledger::raw::Unload();
        That(Env(name) == Wide(std::to_string(GetCurrentProcessId()) + " " + load1 + " 1 " + game1), "an unload by the host leaves the memo: pid, its run, the verdict and the run of the game before");
        That(!Exists(Exit(folder)), "an unload writes no exit mark");
        That(ini::Open(folder), "game 2, the second load");
        const std::string load2 = ledger::Current();
        That(ledger::CleanStart() && ledger::Last() == game1, "a load after an unload in the same game keeps the verdict about game 1");
        That(ledger::Disabled().empty() && ledger::Booted().empty() && ledger::Closed().empty(), "the unloaded load counts as clean: nothing is off");
        That(Env(name).empty(), "the memo is used once");
        That(Logged(folder, "loaded again in the same game: the load before (run " + load1 + ") was unloaded, not crashed; the game before it ended cleanly (run " + game1 + ")"),
             "the log says the add-on was loaded again");
        ledger::Calm();
        ini::Close();
        ledger::raw::Unload();
        That(ini::Open(folder) && ledger::CleanStart() && ledger::Last() == game1 && ledger::Current() != load2, "a third load keeps the verdict too");
        That(ledger::Watch(kWin), "game 2 opens a crash window");
        ledger::raw::Freeze();
        That(Env(name).empty(), "a crash leaves no memo");
        That(ini::Open(folder), "game 3, the first load");
        const std::string game2 = ledger::Last();
        That(!ledger::CleanStart() && ledger::Blocked(kWin), "game 3 sees the crash of game 2 and turns the window off");
        ledger::Calm();
        ini::Close();
        ledger::raw::Unload();
        That(ini::Open(folder), "game 3, the second load");
        That(!ledger::CleanStart() && ledger::Last() == game2 && ledger::Blocked(kWin), "a reload in game 3 still says game 2 did not end cleanly and keeps the window off");
        That(Logged(folder, "the game before it did not end cleanly (run " + game2 + ")"), "the log names the game that did not end cleanly");
        ledger::Retry();
        That(ledger::Watch(kImage), "a load opens a crash window and is unloaded with it still open");
        ini::Close();
        ledger::raw::Unload();
        That(ini::Open(folder) && !ledger::Blocked(kImage) && ledger::Closed() == List{ kImage }, "a window left open by an unloaded load is closed, not turned off");
        const std::string mine = ledger::Current();
        ini::Close();
        SetEnv(name, "1 " + mine + " 1 -");
        That(ini::Open(folder) && !ledger::CleanStart() && ledger::Last() == mine, "a memo of another process is not trusted");
        SetEnv(name, {});
        ini::Close();
        SetEnv(name, std::to_string(GetCurrentProcessId()) + " 0123456789ABCDEF0123456789ABCDEF 1 -");
        That(ini::Open(folder) && !ledger::CleanStart(), "a memo of another run is not trusted");
        SetEnv(name, {});
        ini::Close();
        SetEnv(name, std::to_string(GetCurrentProcessId()) + " " + ledger::Current() + " 2 -");
        That(ini::Open(folder) && !ledger::CleanStart(), "a malformed memo is not trusted");
        SetEnv(name, {});
        ledger::Retry();
    }

    void Child(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "child";
        std::wstring line = L"\"" + Self() + L"\" child \"" + folder.wstring() + L"\"";
        STARTUPINFOW start{};
        start.cb = sizeof(start);
        PROCESS_INFORMATION proc{};
        const bool ran = CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &proc) != 0;
        DWORD code = 99;
        if (ran)
        {
            WaitForSingleObject(proc.hProcess, 60000);
            GetExitCodeProcess(proc.hProcess, &code);
            CloseHandle(proc.hThread);
            CloseHandle(proc.hProcess);
        }
        That(ran && code == 0, "a child run opens, arms a static window, writes the mark and returns from main");
        That(Line(Ini(folder), "watch") == "Probe.Static,Probe.Window", "the static window was not unwatched by the CRT static destructors after the mark");
        That(Exists(Exit(folder)) && scan::Body(Exit(folder)) == Sealed(folder), "the mark still seals the ini after the real process exit");
        That(ini::Open(folder), "the next start opens");
        That(ledger::CleanStart() && ledger::Closed() == List{ "Probe.Static", kWin } && ledger::Disabled().empty(), "the next start sees a clean exit and turns nothing off");
    }

    int Spawned(const std::filesystem::path &folder)
    {
        if (!ini::Open(folder)) return 3;
        lingering = std::make_unique<ledger::Window>("Probe.Static");
        if (!lingering->Armed() || !ledger::Watch(kWin)) return 4;
        ledger::raw::Leave();
        return 0;
    }

    void Crash(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "crash";
        That(ini::Open(folder), "a fresh data folder opens");
        That(ledger::Watch(kWin) && ledger::Watch(kImage), "two windows are open");
        That(ini::Open(folder), "the game crashed, no exit call, the next start opens");
        That(!ledger::CleanStart() && ledger::Closed().empty(), "no mark: no clean exit");
        That(Has(ledger::Booted(), kWin) && ledger::Blocked(kWin) && ledger::Blocked(kImage) && ledger::Disabled() == List{ kWin, kImage }, "the open windows are turned off after the crash");
        That(Logged(folder, "no clean exit mark from the last run"), "the log says there was no clean exit mark");
        ledger::Retry();
        That(!ledger::Blocked(kWin) && ledger::Disabled().empty(), "Retry clears them");
    }

    void After(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "after";
        That(ini::Open(folder), "a fresh data folder opens");
        That(ledger::Watch(kWin), "a window opens");
        ledger::raw::Leave();
        That(ini::Open(folder), "the mark is written and the process dies after it, in another module's exit");
        That(ledger::CleanStart() && ledger::Closed() == List{ kWin } && ledger::Disabled().empty(), "a crash after the mark counts as clean: the engine was already shut down");
        That(ledger::Watch(kWin), "a window opens in the new run");
        ledger::raw::Leave();
        That(Edit(Ini(folder), "watch=Probe.Window\r\n", "watch=Probe.Window,Probe.Image\r\n"), "another program writes the ini after the mark");
        That(ini::Open(folder), "the next start opens");
        That(!ledger::CleanStart() && ledger::Disabled() == List{ kWin, kImage }, "an ini written after the mark makes the mark stale, both windows are off");
        That(Logged(folder, "the exit mark does not match the last run"), "the log says the mark does not match");
        ledger::Retry();
        That(ledger::Watch(kWin), "a window opens again");
        ledger::raw::Leave();
        Put(Ini(folder), scan::Body(Ini(folder)) + "; edited by hand\r\n");
        That(ini::Open(folder), "the next start after a hand edit opens");
        That(!ledger::CleanStart() && ledger::Blocked(kWin), "any change of the ini after the mark, even a comment line, counts as a crash");
        ledger::Retry();
    }

    void Pending(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "pending";
        That(ini::Open(folder), "a fresh data folder opens");
        That(ledger::raw::Arm(kCall) && ledger::Watch(kWin), "a first call is pending while a window is open");
        ledger::raw::Leave();
        That(ini::Open(folder), "the next start opens");
        That(ledger::CleanStart() && ledger::Closed() == List{ kWin } && ledger::Booted() == List{ kCall } && ledger::Blocked(kCall) && !ledger::Blocked(kWin),
             "a call pending at the exit is still turned off, the window is closed");
        ledger::Retry();
    }

    void Series(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "series";
        That(ini::Open(folder), "run 1 starts");
        That(ledger::Watch(kWin), "run 1 opens a window");
        ledger::raw::Leave();
        That(ini::Open(folder), "run 2 starts after a clean exit");
        That(ledger::CleanStart() && ledger::Disabled().empty(), "run 2 starts clean");
        That(ledger::Watch(kWin), "run 2 opens a window");
        That(ini::Open(folder), "run 3 starts after run 2 crashed");
        That(!ledger::CleanStart() && ledger::Disabled() == List{ kWin }, "the mark of run 1 was used by run 2, so the crash of run 2 turns the window off");
        ledger::Retry();
        That(ledger::Disabled().empty() && ledger::Watch(kWin), "Retry, then run 3 opens the window again");
        ledger::raw::Leave();
        That(ini::Open(folder), "run 4 starts after a clean exit");
        That(ledger::CleanStart() && ledger::Disabled().empty() && ledger::Closed() == List{ kWin }, "run 4 starts clean");
        ledger::raw::Leave();
        That(ini::Open(folder), "run 5 starts after a clean exit with nothing open");
        That(ledger::CleanStart() && ledger::Disabled().empty() && ledger::Closed().empty(), "two clean runs in a row change nothing");
        That(Logged(folder, "no crash window was left open"), "the log says nothing was open");
    }

    void Stuck(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "stuck";
        That(ini::Open(folder), "run 1 starts");
        That(ledger::Watch(kWin), "run 1 opens a window");
        ledger::raw::Leave();
        {
            Holder hold(Exit(folder));
            That(hold.held, "another process holds the mark open");
            That(ini::Open(folder), "run 2 starts");
            That(ledger::CleanStart() && ledger::Disabled().empty() && Exists(Exit(folder)) && ledger::Stuck(), "run 2 starts clean, the old mark stays and the diagnostics say so");
            That(Logged(folder, "the old exit mark could not be removed"), "the log says the mark stays");
            That(ledger::Watch(kWin), "run 2 opens a window");
            ledger::raw::Leave();
            That(ini::Open(folder), "run 3 starts after run 2 could not write its mark");
            That(!ledger::CleanStart() && ledger::Disabled() == List{ kWin }, "the old mark names run 1, so run 2 counts as a crash");
        }
        ledger::Retry();
        That(ini::Open(folder), "run 4 starts after the other process let go");
        That(!Exists(Exit(folder)) && !ledger::Stuck(), "the old mark is removed now");
        That(ledger::Watch(kWin), "run 4 opens a window");
        ledger::raw::Leave();
        Locked(Exit(folder));
        That(ini::Open(folder), "run 5 starts with a read-only mark");
        That(ledger::CleanStart() && ledger::Disabled().empty() && !Exists(Exit(folder)) && !ledger::Stuck(), "a read-only mark is made writable and removed, the next exit can mark again");
        That(ledger::Watch(kWin), "run 5 opens a window");
        ledger::raw::Leave();
        That(Exists(Exit(folder)) && scan::Body(Exit(folder)) == Sealed(folder), "run 5 writes its own mark");
        Plain(Exit(folder));
    }

    void Unwritable(const std::filesystem::path &home)
    {
        const std::filesystem::path folder = home / "unwritable";
        That(ini::Open(folder), "run 1 starts");
        That(ledger::Watch(kWin), "run 1 opens a window");
        ledger::raw::Leave();
        Locked(Ini(folder));
        That(ini::Open(folder), "run 2 starts with a read-only ini");
        That(ledger::CleanStart() && !Exists(Exit(folder)) && Logged(folder, "the ini write failed"), "run 2 uses up the mark of run 1 although it cannot write the ini");
        That(ini::Open(folder), "run 3 starts after run 2 crashed without any write");
        That(!ledger::CleanStart() && ledger::Blocked(kWin) && Has(ledger::Booted(), kWin), "the mark of run 1 is gone, so the crash of run 2 is not hidden");
        ledger::Retry();
        Plain(Ini(folder));
        That(ini::Open(folder), "run 4 starts with the ini writable again");
        ledger::Retry();
        That(ledger::Disabled().empty() && ledger::Watch(kWin), "run 4 clears the old crash and opens a window");
        ledger::raw::Leave();
        {
            Holder hold(Exit(folder));
            Locked(Ini(folder));
            That(hold.held && ini::Open(folder), "run 5 starts: the mark cannot be removed and the ini cannot be written");
            That(!ledger::CleanStart() && ledger::Closed().empty() && ledger::Blocked(kWin) && ledger::Stuck(), "a mark that can be neither used up nor outdated is not trusted");
            That(Logged(folder, "can be neither removed nor outdated by a new ini"), "the log says why the mark is not trusted");
        }
        Plain(Ini(folder));
        ledger::Retry();
    }

    void Older(const std::filesystem::path &home)
    {
        const std::filesystem::path old = home / "old";
        Put(Ini(old), "[general]\r\nlanguage=uk\r\n[safety]\r\npending=Probe.Call\r\ndisabled=Probe.Gone\r\nwatch=Probe.Window,Probe.Image\r\n");
        That(ini::Open(old), "an ini of an older ZoneSDK, without a session line, opens");
        That(!ledger::CleanStart() && !ledger::Blocked(kWin) && !ledger::Blocked(kImage) && !ledger::Blocked(kCall) && ledger::Booted().empty(),
             "an ini of an older ZoneSDK: its crash markers are not judged, nothing new is disabled");
        That(ledger::Blocked("Probe.Gone") && ledger::Disabled() == List{ "Probe.Gone" }, "what the older ZoneSDK disabled itself stays disabled");
        That(Line(Ini(old), "pending").empty() && Line(Ini(old), "watch").empty() && ledger::Session(Line(Ini(old), "session")), "the markers are cleared once and the ini now has a session");
        That(Logged(old, "not judged, written by an older ZoneSDK") && Logged(old, "Probe.Call,Probe.Window,Probe.Image"), "the log says the run was not judged and names the cleared markers");
        That(ledger::Watch(kWin), "the new run opens a window");
        That(ini::Open(old), "the new run crashed, the next start opens");
        That(!ledger::CleanStart() && ledger::Blocked(kWin), "the excuse holds once: the crash of the first new run is judged");
        ledger::Retry();
        const std::filesystem::path folder = home / "older";
        That(ini::Open(folder), "a new run starts");
        That(ledger::Watch(kWin), "it opens a window");
        ledger::raw::Leave();
        const std::filesystem::path file = Ini(folder);
        That(Edit(file, "disabled=\r\nwatch=Probe.Window\r\n", "disabled=Probe.Window\r\nwatch=Probe.Image\r\n"), "an older version starts next, turns the window off and crashes with its own window open");
        That(Line(file, "session") != "?", "the older version keeps the session line it does not know");
        That(ini::Open(folder), "the new version starts again");
        That(!ledger::CleanStart() && ledger::Blocked(kImage) && ledger::Blocked(kWin), "the mark of the new version does not hide the crash of the older version");
        ledger::Retry();
        That(ini::Open(folder), "the new version starts and quits cleanly");
        ledger::raw::Leave();
        const std::string session = Line(file, "session");
        std::string body = scan::Body(file);
        const std::string line = "session=" + session + "\r\n";
        const size_t at = body.find(line);
        if (at != std::string::npos) body.erase(at, line.size());
        const std::string empty = "watch=\r\n";
        const size_t watch = body.find(empty);
        if (watch != std::string::npos) body.replace(watch, empty.size(), "watch=" + std::string(kWin) + "\r\n" + line);
        Put(file, body);
        That(at != std::string::npos && watch != std::string::npos, "the older version rewrites the ini as it does on every start: the session after watch, and crashes with a window open");
        That(ini::Open(folder), "the new version starts after that crash");
        That(!ledger::CleanStart() && ledger::Blocked(kWin) && Logged(folder, "the exit mark does not match the last run " + session), "the mark does not vouch for an ini the older version rewrote");
        ledger::Retry();
    }

    std::string Of(const std::string &code, const std::string &name, const std::string &inside = {})
    {
        for (const scan::Function &f : scan::Functions(code))
            if (f.name == name && (inside.empty() || f.body.find(inside) != std::string::npos)) return f.body;
        return {};
    }

    bool First(const std::string &body, const std::string &guard, const std::string &rest)
    {
        const size_t at = body.find(guard);
        const size_t later = body.find(rest);
        return at != std::string::npos && (later == std::string::npos || at < later);
    }

    std::string Shared(const std::string &header)
    {
        const size_t from = header.find("namespace zsdk::ledger\n{");
        const size_t to = header.find("namespace zsdk::ledger::raw");
        return from == std::string::npos || to == std::string::npos ? std::string() : header.substr(from, to - from);
    }

    void Sources()
    {
        const std::filesystem::path sdk = scan::SdkRoot();
        const std::string shell = scan::Lex(scan::Body(sdk / "src" / "entry" / "shell.cpp")).code;
        const size_t branch = shell.find("reason == DLL_PROCESS_DETACH && live");
        const size_t leave = branch == std::string::npos ? branch : shell.find("if (reserved && ledger::Ended(zsdk::thread::OnGame(), runtimes.load(), dropped.load())) ledger::raw::Leave();", branch);
        const size_t freeze = branch == std::string::npos ? branch : shell.find("else if (reserved) ledger::raw::Freeze();", branch);
        const size_t dusk = branch == std::string::npos ? branch : shell.find("detail::engine::Dusk();", branch);
        That(branch != std::string::npos && leave != std::string::npos && freeze != std::string::npos && dusk != std::string::npos && leave < freeze && freeze < dusk,
             "the process exit writes the mark first, only on the game thread after the last swap chain went, and freezes the ledger otherwise");
        That(scan::Count(shell, "ledger::raw::Leave()") == 1 && scan::Count(shell, "ledger::raw::Freeze()") == 1, "the mark is written in one place only");
        const size_t flag = branch == std::string::npos ? branch : shell.find("if (reserved) detail::Exit(true);", branch);
        That(flag != std::string::npos && flag < leave && shell.find(";", branch) > flag, "a process exit sets the exit flag first, before the mark");
        That(shell.find("bool Exiting()") != std::string::npos && shell.find("return detail::Exiting();") != std::string::npos, "shell::Exiting tells hooks.stop that the process exits");
        const std::string threadCode = scan::Lex(scan::Body(sdk / "src" / "present" / "thread.cpp")).code;
        const std::string stop = Of(threadCode, "Stop");
        That(First(stop, "if (detail::Exiting()) return;", "WaitForSingleObject(worker, 200)"), "at the process exit thread::Stop does not wait for the worker, which is already gone");
        const size_t unload = shell.find("if (!reserved)", branch == std::string::npos ? 0 : branch);
        That(unload != std::string::npos && shell.find("ledger::Calm();", unload) != std::string::npos, "an unload by FreeLibrary still closes the windows itself");
        That(shell.find("register_event<reshade::addon_event::init_effect_runtime>(&OnMakeRuntime)") != std::string::npos && shell.find("unregister_event<reshade::addon_event::init_effect_runtime>(&OnMakeRuntime)") != std::string::npos,
             "the swap chain count follows init_effect_runtime");
        const std::string drop = Of(shell, "OnDropRuntime");
        That(drop.find("runtimes.compare_exchange_weak") != std::string::npos && drop.find("dropped = true;") != std::string::npos && First(drop, "if (stopping) return;", "ini::Save()"),
             "destroy_effect_runtime counts the swap chain down and saves nothing once the exit began");
        const std::string present = Of(shell, "OnPresent");
        That(!present.empty() && present.find("runtimes") == std::string::npos && present.find("dropped") == std::string::npos && present.find("Leave") == std::string::npos, "nothing new in reshade_present");
        const std::string ini = scan::Lex(scan::Body(sdk / "src" / "present" / "ini.cpp")).code;
        const std::string body = Of(ini, "Leave");
        That(!body.empty(), "the exit writer is found");
        for (const char *word : { "mutex", "lock_guard", "unique_lock", "Log(", "Rare(", "Note(", "Flush(", "std::string", "wstring", "filesystem", "new ", "LoadLibrary", "MoveFile", "WriteNow", "Mark(",
                                  "Seal(", "Joined(", "push_back", "ofstream", "malloc", "Sleep", "Wait" })
            That(body.find(word) == std::string::npos, std::string("the exit writer takes no lock, allocates nothing and logs nothing: no ") + word);
        for (const char *word : { "ini::frozen.store(true)", "slipAt.exchange(-1)", "::CreateFileW(", "CREATE_ALWAYS", "::WriteFile(", "CloseHandle(" })
            That(body.find(word) != std::string::npos, std::string("the exit writer is one small file write: ") + word);
        That(First(body, "ini::frozen.store(true)", "slipAt"), "the exit freezes the ledger before it writes the mark");
        That(First(Of(ini, "WriteNow"), "if (frozen.load()) return false;", "lock_guard") && First(Of(ini, "SaveSettings"), "if (frozen.load()) return false;", "lock_guard"),
             "after the mark the ini is not written, no lock is taken");
        That(First(Of(ini, "Window", "Unwatch(key, std::string())"), "if (ini::frozen.load()) return;", "Unwatch("), "a window destroyed after the mark writes nothing");
        const size_t ctor = ini.find("Window::Window(std::string wanted)");
        const size_t refuse = ctor == std::string::npos ? ctor : ini.find("ini::frozen.load()", ctor);
        const size_t blocked = ctor == std::string::npos ? ctor : ini.find("if (Blocked(key))", ctor);
        That(ctor != std::string::npos && refuse != std::string::npos && blocked != std::string::npos && refuse < blocked, "a window asked for after the mark is refused first");
        That(First(Of(ini, "Close", "armed = false"), "!ini::frozen.load()", "Unwatch("), "a window closed after the mark writes nothing");
        const std::pair<const char *, const char *> guarded[] = { { "Watch", "WatchLimit()" },{ "Unwatch", "Built(build)" }, { "Calm", "kept.watch.clear()" }, { "Retry", "booted.clear()" }, { "Store", "kept.owned" },
                                                                  { "Put", "OwnedOf(" }, { "Arm", "ini::inflight = key" }, { "Prove", "kept.proven[key] = build" }, { "Settle", "inflight.clear()" }, { "Disable", "ini::Keep(" } };
        for (const auto &[name, inside] : guarded)
            That(First(Of(ini, name, inside), "frozen.load()", "lock_guard"), std::string("after the mark ") + name + " takes no lock and writes nothing");
        const std::string open = Of(ini, "Open", "ledger::Judge(");
        That(First(open, "const bool removed = Drop(stop, error);", "WriteAtomic(base / id.ini, body)") && open.find("const bool doubt = left.clean && !removed && !wrote;") != std::string::npos,
             "the mark is used up before the ini is written, and a mark that cannot be used up is not trusted without a new ini");
        That(open.find("ledger::Seal(text)") != std::string::npos && Of(ini, "Publish").find("ledger::Seal(written)") != std::string::npos, "the mark seals the whole ini as written and as read back");
        That(First(open, "frozen.store(false);", "ledger::Judge("), "a new start lifts the freeze of the run before");
        const std::string thread = scan::Lex(scan::Body(sdk / "src" / "present" / "thread.cpp")).code;
        That(thread.find("msg.message == WM_QUIT") != std::string::npos && thread.find("ledger::Calm();") != std::string::npos, "the WM_QUIT path of other hosts stays");
        const std::string publicHeader = scan::Lex(scan::Body(sdk / "include" / "zsdk" / "ledger.h")).code;
        for (const char *word : { "Leftover", "Session(", "Seal(", "Mark(", "Marked(", "Judge(", "Ended(", "kSessionMost", "kMarkMost" })
            That(publicHeader.find(word) == std::string::npos, std::string("the public ledger header does not carry the exit internals: ") + word);
        for (const char *word : { "bool CleanStart();", "bool Stuck();", "std::vector<std::string> Closed();", "std::string Current();", "std::string Last();" })
            That(publicHeader.find(word) != std::string::npos, std::string("the public ledger header tells how the last run ended: ") + word);
        That(scan::Lex(scan::Body(sdk / "include" / "zsdk" / "ini.h")).code.find("std::filesystem::path ExitPath();") != std::string::npos, "ini::ExitPath is public");
        const std::string detailRaw = scan::Body(sdk / "src" / "detail" / "ledger_raw.h");
        const std::string testRaw = scan::Body(sdk / "include" / "zsdk" / "test" / "ledger_raw.h");
        That(!Shared(detailRaw).empty() && Shared(detailRaw) == Shared(testRaw), "the test ledger header repeats the internal exit declarations byte for byte");
        That(detailRaw.find("void Freeze();") != std::string::npos && detailRaw.find("void Leave();") != std::string::npos && testRaw.find("void Leave();") != std::string::npos, "Freeze and Leave are raw ledger calls");
        That(detailRaw.find("void Unload();") != std::string::npos && testRaw.find("void Unload();") != std::string::npos, "Unload is a raw ledger call");
        const size_t memo = unload == std::string::npos ? unload : shell.find("ledger::raw::Unload();", unload);
        const size_t saved = unload == std::string::npos ? unload : shell.find("ini::Close();", unload);
        That(memo != std::string::npos && saved != std::string::npos && saved < memo && scan::Count(shell, "ledger::raw::Unload()") == 1, "an unload by FreeLibrary leaves the reload memo once, after the ini is saved");
        const std::string reload = Of(ini, "Unload");
        That(!reload.empty() && First(reload, "ini::frozen.load()", "SetEnvironmentVariableW"), "a frozen ledger leaves no reload memo");
    }
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 3 && std::wstring(argv[1]) == L"child") return Spawned(std::filesystem::path(argv[2]));
    if (argc == 5 && std::wstring(argv[1]) == L"hold") return Hold(std::filesystem::path(argv[2]), argv[3], argv[4]);
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    std::error_code code;
    for (const char *one : { "z", "clean", "frozen", "freeze", "child", "crash", "after", "pending", "series", "stuck", "unwritable", "old", "older", "reload" }) std::filesystem::remove_all(home / one, code);
    Unarmed(home);
    Pure();
    Text();
    Clean(home);
    Frozen(home);
    Freeze(home);
    Child(home);
    Crash(home);
    After(home);
    Pending(home);
    Series(home);
    Stuck(home);
    Unwritable(home);
    Older(home);
    Reload(home);
    Sources();
    return check::Finish("exit");
}
