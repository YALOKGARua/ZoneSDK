#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/ledger_raw.h>
#include <zsdk/test/scan.h>

#include <windows.h>

#include "support/probe.h"

#include <algorithm>
#include <string>

namespace
{
    using check::That;
    namespace ini = zsdk::ini;
    namespace ledger = zsdk::ledger;

    std::string Safety(const std::filesystem::path &folder)
    {
        const std::string body = scan::Body(folder / "probe.ini");
        const size_t from = body.find("[safety]");
        const size_t to = body.find("[proven]");
        return from == std::string::npos || to == std::string::npos ? std::string() : body.substr(from, to - from);
    }

    std::string Head()
    {
        return "[safety]\r\nsession=" + ledger::Current() + "\r\n";
    }

    std::string Tail()
    {
        return "addon=" + ledger::Addon() + "\r\n\r\n";
    }

    void Builds(const std::filesystem::path &folder)
    {
        const std::string self = ledger::Addon();
        That(self.rfind("0.0.1/", 0) == 0 && self.size() == 14, "the add-on build is its version and the stamp of its image: " + self);
        const std::string mine = "[general]\r\nlanguage=uk\r\n[safety]\r\naddon=" + self + "\r\n[proven]\r\nKeep.Me=61bc1e03\r\n";
        That(ini::WriteAtomic(folder / "probe.ini", mine) && ini::Open(folder) && ledger::Proven("Keep.Me") == "61bc1e03", "proofs written by this very build are kept");
        const std::string other = "[general]\r\nlanguage=uk\r\n[safety]\r\naddon=0.0.0/00000000\r\n[proven]\r\nOld.Call=61bc1e03\r\nOld.Window=61bc1e03\r\n";
        That(ini::WriteAtomic(folder / "probe.ini", other) && ini::Open(folder), "a boot after an update of the add-on");
        That(ledger::Proven("Old.Call").empty() && ledger::Proven("Old.Window").empty() && ini::Ledger().proven.empty(), "an update of the add-on drops every proof, so first calls are marked again");
        zsdk::Flush();
        const std::string log = scan::Body(folder / "probe.log");
        That(log.find("the add-on build changed from 0.0.0/00000000 to " + self + ", 2 proofs are dropped") != std::string::npos, "the drop is logged with both builds");
        That(scan::Body(folder / "probe.ini").find("addon=" + self + "\r\n") != std::string::npos, "the new build is written into [safety]");
        const std::string unmarked = "[general]\r\nlanguage=uk\r\n[proven]\r\nOlder.Call=61bc1e03\r\n";
        That(ini::WriteAtomic(folder / "probe.ini", unmarked) && ini::Open(folder) && ledger::Proven("Older.Call").empty(), "proofs of an ini that names no add-on build are dropped once");
        zsdk::Flush();
        That(scan::Body(folder / "probe.log").find("the add-on build changed from an unrecorded build") != std::string::npos, "an unrecorded build is named in the log");
        That(ledger::raw::Arm("Fresh.Call") && ledger::raw::Prove("Fresh.Call", "61bc1e03") && ini::Open(folder) && ledger::Proven("Fresh.Call") == "61bc1e03", "a proof of the current build survives the next boot");
    }

    void Windows(const std::filesystem::path &folder)
    {
        zsdk::App &app = probe::Mutable();
        That(ledger::WatchLimit() == 8 && zsdk::ini::Limits::kWatchCap == 32, "the default watch limit is 8 of a cap of 32");
        app.policy.watchKept = 32;
        That(!zsdk::Legacy(app.policy), "a raised watch limit is not a legacy policy");
        That(ledger::WatchLimit() == 32 && ini::Open(folder), "watchKept 32 is taken");
        for (int i = 0; i < 40; ++i) ledger::Watch("Wide." + std::to_string(i));
        const auto watched = ledger::Watched();
        That(watched.size() == 32 && watched.front() == "Wide.0" && watched.back() == "Wide.31", "the ledger keeps 32 open windows with watchKept 32 and refuses the 33rd");
        That(ini::Open(folder) && ledger::Disabled().size() == 32 && ledger::Blocked("Wide.31"), "32 windows open at a crash are all read back and disabled");
        ledger::Retry();
        app.policy.watchKept = 33;
        That(ledger::WatchLimit() == 8 && ini::Open(folder), "watchKept above the cap is refused, the default 8 applies");
        zsdk::Flush();
        That(scan::Body(folder / "probe.log").find("policy watchKept 33 is refused, it must be 1 to 32, the ledger keeps 8 windows") != std::string::npos, "the refusal is logged at load");
        for (int i = 0; i < 12; ++i) ledger::Watch("Narrow." + std::to_string(i));
        That(ledger::Watched().size() == 8, "with a refused watchKept the ledger keeps 8 windows");
        That(ledger::Calm(), "calm");
        app.policy.watchKept = 0;
        That(ledger::WatchLimit() == 8, "watchKept 0 is refused as well");
        app.policy.watchKept = 8;
    }

    std::string Proven(const std::filesystem::path &folder)
    {
        const std::string body = scan::Body(folder / "probe.ini");
        const size_t from = body.find("[proven]");
        const size_t to = body.find("[probe]");
        return from == std::string::npos || to == std::string::npos ? std::string() : body.substr(from, to - from);
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path folder = home / "ledger";
    std::error_code code;
    std::filesystem::remove_all(folder, code);
    That(ini::Open(folder), "the folder opens");
    That(Safety(folder) == Head() + "pending=\r\ndisabled=\r\nwatch=\r\n" + Tail(), "a fresh ledger is empty");
    That(ledger::raw::Arm("Grenade.Explode"), "arm writes through");
    That(Safety(folder) == Head() + "pending=Grenade.Explode\r\ndisabled=\r\nwatch=\r\n" + Tail(), "the pending key is in the ini before the call");
    That(ledger::raw::Prove("Grenade.Explode", "61bc1e03"), "prove writes through");
    That(Safety(folder) == Head() + "pending=\r\ndisabled=\r\nwatch=\r\n" + Tail() && Proven(folder) == "[proven]\r\nGrenade.Explode=61bc1e03\r\n\r\n", "prove clears pending and stores the build");
    That(ledger::raw::Arm("X.Settled") && ledger::raw::Settle() && Safety(folder).find("pending=\r\n") != std::string::npos, "settle clears a C++ error without blame");
    That(ledger::raw::Arm("X.Crash"), "a call that never returns stays pending");
    That(ini::Open(folder), "the next boot");
    That(ledger::Blocked("X.Crash") && ledger::Booted().size() == 1 && ledger::Booted()[0] == "X.Crash", "the pending key is disabled at boot and reported");
    That(Safety(folder) == Head() + "pending=\r\ndisabled=X.Crash\r\nwatch=\r\n" + Tail(), "the boot writes the disable");
    That(ledger::Proven("Grenade.Explode") == "61bc1e03", "proven keys survive the boot");
    That(ledger::raw::Disable("Y.Fault") && ledger::Blocked("Y.Fault") && Safety(folder).find("disabled=X.Crash,Y.Fault") != std::string::npos, "a native fault disables at once");
    That(ledger::Watch("Picker.Types") && ledger::Watch("Picker.Throw") && ledger::Watch("Picker.Types") && ledger::Watched().size() == 2, "watch windows do not repeat");
    That(Safety(folder).find("watch=Picker.Types,Picker.Throw") != std::string::npos, "the watch list is written through");
    That(ledger::Unwatch("Picker.Types", "61bc1e03") && ledger::Proven("Picker.Types") == "61bc1e03" && ledger::Watched().size() == 1, "a closed window is proven");
    That(ledger::Unwatch("Never.Opened", "") && !ledger::Blocked("Never.Opened"), "closing a window that was never open is harmless");
    That(ini::Open(folder), "a boot after a run that ended inside a window");
    That(ledger::Blocked("Picker.Throw") && ledger::Watched().empty(), "a window open at the end of the run is disabled");
    const auto booted = ledger::Booted();
    That(std::find(booted.begin(), booted.end(), "Picker.Throw") != booted.end(), "the boot reports the window");
    That(ledger::Watch("Picker.Load") && ledger::Calm() && ledger::Watched().empty() && Safety(folder).find("watch=\r\n") != std::string::npos, "a clean quit calms every window");
    That(ledger::Calm(), "calming twice is harmless");
    That(ini::Open(folder) && !ledger::Blocked("Picker.Load"), "the boot after a clean quit disables nothing new");
    ledger::Retry();
    That(ledger::Disabled().empty() && ledger::Booted().empty() && Safety(folder) == Head() + "pending=\r\ndisabled=\r\nwatch=\r\n" + Tail(), "retry clears the ledger");
    That(ledger::Proven("Grenade.Explode") == "61bc1e03", "retry keeps the proven keys");
    for (int i = 0; i < 40; ++i) ledger::raw::Disable("K." + std::to_string(i));
    That(ledger::Disabled().size() == 32 && ledger::Full(), "the disabled list is capped at 32");
    That(ledger::Blocked("K.39") && ledger::Blocked("K.8") && !ledger::Blocked("K.7") && !ledger::Blocked("K.0"), "a full list keeps the newest keys, the newest fault is never lost");
    That(ledger::raw::Arm("K.40") && ini::Open(folder) && ledger::Blocked("K.40") && ledger::Disabled().size() == 32 && !ledger::Blocked("K.8"), "a crash key pending at boot is kept on a full list");
    ledger::Retry();
    That(ledger::Disabled().empty() && !ledger::Full(), "retry empties the full list");
    That(ledger::raw::Arm("In.Flight"), "a call is in flight");
    ledger::Retry();
    That(Safety(folder).find("pending=In.Flight\r\n") != std::string::npos, "retry keeps the marker of a call that is still running");
    That(ledger::raw::Prove("In.Flight", "61bc1e03") && Safety(folder).find("pending=\r\n") != std::string::npos, "the call returns and clears its own marker");
    That(ledger::Watch("Live.Window"), "a window is open");
    ledger::Retry();
    That(ledger::Watched().size() == 1 && Safety(folder).find("watch=Live.Window\r\n") != std::string::npos, "retry keeps the watch of an open window");
    That(ledger::Unwatch("Live.Window", ""), "the window closes");
    That(ledger::raw::Arm("Q.Mark") && ledger::raw::Prove("Q.Mark", "?") && ledger::Proven("Q.Mark").empty() && Safety(folder).find("pending=\r\n") != std::string::npos,
         "a call without a known build returns but is not proven");
    That(ledger::raw::Arm("Q.Empty") && ledger::raw::Prove("Q.Empty", "") && ledger::Proven("Q.Empty").empty(), "an empty build proves nothing");
    for (int i = 0; i < 10; ++i) ledger::Watch("W." + std::to_string(i));
    That(ledger::Watched().size() == 8, "the watch list is capped at 8");
    That(ledger::Calm(), "calm");
    {
        ledger::Window window("Window.Scoped");
        That(window.Armed() && window.Open() && !window.Refused() && ledger::Watched().size() == 1, "a window watches its key while it is open");
        window.Close();
        That(ledger::Watched().empty() && ledger::Proven("Window.Scoped").empty(), "closing a window without a known build proves nothing");
    }
    {
        ledger::Window window("Window.Dropped");
        That(window.Armed(), "a second window is armed");
    }
    That(ledger::Watched().empty() && ledger::Proven("Window.Dropped").empty(), "a window left without Close is unwatched without proof");
    That(ledger::raw::Disable("Window.Crashed"), "a window key is disabled");
    {
        ledger::Window window("Window.Crashed");
        That(window.Refused() && !window.Armed() && ledger::Watched().empty(), "a disabled window refuses and watches nothing");
    }
    for (int i = 0; i < 8; ++i) ledger::Watch("Fill." + std::to_string(i));
    {
        ledger::Window window("Window.NoRoom");
        That(window.Refused() && !window.Armed(), "a window that cannot be watched refuses");
    }
    That(ledger::Calm(), "calm again");
    const std::string fixture = "[general]\r\nlanguage=uk\r\n[safety]\r\naddon=" + ledger::Addon() + "\r\n[proven]\r\nOld.Q=?\r\nOld.Empty=\r\nOld.Good=61bc1e03\r\n";
    That(ini::WriteAtomic(folder / "probe.ini", fixture) && ini::Open(folder), "a ledger with proofs of an unknown build");
    That(ledger::Proven("Old.Q").empty() && ledger::Proven("Old.Empty").empty() && ledger::Proven("Old.Good") == "61bc1e03", "the boot drops proofs without a known build");
    const std::wstring file = (folder / "probe.ini").wstring();
    const DWORD was = GetFileAttributesW(file.c_str());
    SetFileAttributesW(file.c_str(), was | FILE_ATTRIBUTE_READONLY);
    const bool armed = ledger::raw::Arm("Locked.Ini");
    SetFileAttributesW(file.c_str(), was);
    That(!armed, "arming reports a crash marker that could not be written");
    That(ledger::raw::Settle() && Safety(folder).find("pending=\r\n") != std::string::npos, "settling after a refused arm leaves no marker");
    That(!ledger::Preloaded("Grenade.Explode") && !ledger::Preloaded("Grenade.Explode", "61bc1e03"), "no key is preloaded without a proven.txt row");
    Builds(folder);
    Windows(folder);
    return check::Finish("ledger");
}
