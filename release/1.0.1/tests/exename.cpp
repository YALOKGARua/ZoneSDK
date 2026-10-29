#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/native_bindings.h"
#include "../src/detail/runtime.h"

#include <windows.h>

#include <string>

namespace
{
    using check::That;

    template<class F> bool Within(int ms, F &&done)
    {
        for (int waited = 0; waited < ms; waited += 20)
        {
            if (done()) return true;
            Sleep(20);
        }
        return done();
    }
}

int main()
{
    namespace exe = zsdk::exe;
    That(exe::Game(L"Stalker2-Win64-Shipping.exe") && exe::Game(L"Stalker2-WinGDK-Shipping.exe") && exe::Game(L"stalker2-wingdk-shipping.EXE"), "every Stalker2-*-Shipping.exe is the game");
    That(!exe::Game(L"Stalker2-Shipping.exe") && !exe::Game(L"Stalker2--Shipping.exe") && !exe::Game(L"Stalker2-Win 64-Shipping.exe") && !exe::Game(L"Stalker2-Win64-Shipping.exe.bak") &&
             !exe::Game(L"Stalker2.exe") && !exe::Game(L"notepad.exe") && !exe::Game(L""),
         "other names are not the game");
    That(exe::Steam(L"Stalker2-Win64-Shipping.exe") && exe::Steam(L"STALKER2-WIN64-SHIPPING.EXE") && !exe::Steam(L"Stalker2-WinGDK-Shipping.exe") && !exe::Steam(L"Stalker2-Win64-Test.exe"),
         "only Stalker2-Win64-Shipping.exe is the Steam build");
    That(!exe::Steam() && exe::Name().rfind("zsdk-exename", 0) == 0, "the suite exe is neither the game nor the Steam build: " + exe::Name());
    zsdk::detail::Pretend(L"Stalker2-WinGDK-Shipping.exe");
    That(exe::Name() == "Stalker2-WinGDK-Shipping.exe" && exe::Game(exe::File()) && !exe::Steam(), "the suite poses as another store build of the game");
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path folder = home / "exename";
    std::error_code code;
    std::filesystem::remove_all(folder, code);
    That(zsdk::ini::Open(folder), "the data folder opens");
    zsdk::detail::thread::Start();
    That(Within(3000, [] { return zsdk::thread::Rejected(); }), "a game exe that is not the Steam build is refused on the first census pass");
    That(zsdk::detail::native_bindings::State() == zsdk::detail::native_bindings::Status::Pending, "no pattern scan of game memory ran");
    That(!zsdk::thread::Bound() && !zsdk::thread::Booked() && zsdk::thread::State(true) == zsdk::Phase::Failed, "nothing is bound or booked and the phase is Failed, as on a rejected build");
    That(exe::Sha().empty() && !exe::Known(), "the exe is not hashed and no build is known");
    bool toasted = false;
    for (const zsdk::toast::Item &item : zsdk::toast::Live(GetTickCount64()))
        toasted = toasted || (item.kind == zsdk::toast::kUnsupported && item.detail.find("Stalker2-WinGDK-Shipping.exe is not the Steam build") != std::string::npos);
    That(toasted, "an SDK toast says that this game build is not supported");
    That(std::string(zsdk::lang::Text("sdk.unsupported", zsdk::lang::Lang::En)) == "This game build is not supported, the add-on changes nothing" &&
             std::string(zsdk::lang::Text("sdk.unsupported", zsdk::lang::Lang::Uk)).find("не підтримується") != std::string::npos,
         "the toast and the tab line have Ukrainian and English text");
    const uint64_t turned = zsdk::detail::thread::Turns();
    Sleep(400);
    That(zsdk::detail::thread::Turns() > turned, "the census loop keeps ticking without engine work");
    zsdk::Flush();
    const std::string log = scan::Body(zsdk::ini::LogPath());
    That(log.find("this game build is not supported: Stalker2-WinGDK-Shipping.exe is not the Steam build Stalker2-Win64-Shipping.exe, game calls stay off for this session") != std::string::npos,
         "the log says why the build is refused");
    That(log.find("native bindings") == std::string::npos && log.find("engine bound") == std::string::npos, "no binding was tried");
    zsdk::detail::thread::Stop();
    return check::Finish("exename");
}
