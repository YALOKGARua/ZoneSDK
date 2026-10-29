#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/engine_core.h"
#include "../src/detail/runtime.h"
#include "support/fake.h"

#include <windows.h>

#include <cstring>
#include <string>

namespace
{
    using check::That;
    namespace ue = zsdk::ue;
    namespace engine = zsdk::detail::engine;

    constexpr uint64_t kConstParm = 0x2;

    template<class T> void Poke(uint8_t *at, int32_t offset, const T &value)
    {
        std::memcpy(at + offset, &value, sizeof(T));
    }

    void Param(fake::Game &game, uint8_t *fn, uint8_t *&tail, const char *name, const char *type, int32_t at, int32_t size, uint64_t flags)
    {
        uint8_t *prop = game.Loose();
        uint8_t *kind = game.Loose();
        const uint32_t kindName[2] = { game.Name(type), 0 };
        std::memcpy(kind, kindName, sizeof(kindName));
        Poke(prop, 0x08, static_cast<const void *>(kind));
        const uint32_t pair[2] = { game.Name(name), 0 };
        std::memcpy(prop + fake::kPropertyNameAt, pair, sizeof(pair));
        Poke(prop, fake::kPropertyOffsetAt, at);
        Poke(prop, fake::kPropertyOffsetAt - 0x10, size);
        Poke(prop, fake::kPropertyOffsetAt - 0x0C, flags);
        if (tail) Poke(tail, fake::kPropertyNextAt, static_cast<const void *>(prop));
        else Poke(fn, fake::kPropertiesAt, static_cast<const void *>(prop));
        tail = prop;
    }

    template<class F> std::string Refusal(F &&work)
    {
        try
        {
            work();
            return "passed";
        }
        catch (const zsdk::Refused &error)
        {
            return std::string("refused: ") + error.what();
        }
        catch (const std::exception &error)
        {
            return std::string("error: ") + error.what();
        }
    }

    HWND Window()
    {
        WNDCLASSEXW kind{};
        kind.cbSize = sizeof(kind);
        kind.lpfnWndProc = DefWindowProcW;
        kind.hInstance = GetModuleHandleW(nullptr);
        kind.lpszClassName = L"UnrealWindow";
        RegisterClassExW(&kind);
        return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"UnrealWindow", L"zsdk call lend", WS_POPUP | WS_VISIBLE, -32000, -32000, 64, 64, nullptr, nullptr, kind.hInstance, nullptr);
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    zsdk::log::Open(home / "calllend.log", home / "calllend.1.log");
    fake::Game game;
    uint8_t *actor = game.Type("Actor", game.object);
    uint8_t *alive = game.Make("BP_Alive", actor, nullptr);
    uint8_t *garbage = game.Make("BP_Garbage", actor, nullptr);
    game.ItemFlags(garbage, ue::kItemGarbage);
    uint8_t *tail = nullptr;
    uint8_t *getLabel = game.Make("XGetLabel", game.function, actor);
    Param(game, getLabel, tail, "Text", "StrProperty", 0, 16, engine::kParm | engine::kOutParm | engine::kReferenceParm);
    tail = nullptr;
    uint8_t *outLabel = game.Make("XOutLabel", game.function, actor);
    Param(game, outLabel, tail, "Text", "StrProperty", 0, 16, engine::kParm | engine::kOutParm);
    tail = nullptr;
    uint8_t *named = game.Make("XNamed", game.function, actor);
    Param(game, named, tail, "ReturnValue", "StrProperty", 0, 16, engine::kParm | engine::kOutParm | engine::kReturnParm);
    tail = nullptr;
    uint8_t *setLabel = game.Make("XSetLabel", game.function, actor);
    Param(game, setLabel, tail, "Text", "StrProperty", 0, 16, engine::kParm | kConstParm | engine::kReferenceParm);
    tail = nullptr;
    uint8_t *byValue = game.Make("XSay", game.function, actor);
    Param(game, byValue, tail, "Text", "StrProperty", 0, 16, engine::kParm);
    tail = nullptr;
    uint8_t *aim = game.Make("XAimAt", game.function, actor);
    Param(game, aim, tail, "Target", "ObjectProperty", 0, 8, engine::kParm);
    engine::Restock();
    That(engine::Exact("XGetLabel").size() == 1 && engine::Exact("XSetLabel").size() == 1 && engine::Exact("XAimAt").size() == 1, "the fake function book has the functions");

    zsdk::detail::thread::Start();
    const HWND window = Window();
    zsdk::detail::thread::Present();
    That(zsdk::thread::Hooked() && zsdk::thread::OnGame(), "the suite thread is the hooked game thread");
    {
        zsdk::detail::thread::Scope pump;
        const std::wstring name = L"Strelok";
        const std::string inout = Refusal([&] {
            zsdk::Call c("Actor", "XGetLabel", alive);
            c.Set("Text", ue::Lend(name));
        });
        That(inout.rfind("refused: parameter Text of Actor.XGetLabel is an FString the game writes back", 0) == 0,
             "a borrowed string lent into an in-out FString parameter (CPF_OutParm|CPF_ReferenceParm) is refused, the game would free or reallocate the add-on's buffer: " + inout);
        That(Refusal([&] {
                 zsdk::Call c("Actor", "XOutLabel", alive);
                 c.Set("Text", ue::Lend(name));
             }).rfind("refused: ", 0) == 0,
             "a borrowed string lent into an out FString parameter (CPF_OutParm) is refused");
        That(Refusal([&] {
                 zsdk::Call c("Actor", "XNamed", alive);
                 c.Set("ReturnValue", ue::Lend(name));
             }).rfind("refused: ", 0) == 0,
             "a borrowed string lent into an FString return value is refused");
        std::wstring back;
        That(Refusal([&] {
                 zsdk::Call c("Actor", "XSetLabel", alive);
                 c.Set("Text", ue::Lend(name));
                 back = c.CopyText("Text");
             }) == "passed" &&
                 back == name,
             "a const FString& parameter (CPF_ConstParm|CPF_ReferenceParm, no CPF_OutParm) takes the borrowed string and reads back the same text");
        That(Refusal([&] {
                 zsdk::Call c("Actor", "XSay", alive);
                 c.Set("Text", ue::Lend(L"XDestroyModel"));
             }) == "passed",
             "an FString passed by value takes a lent literal");
        That(Refusal([&] {
                 zsdk::Call c("Actor", "XAimAt", alive);
                 c.Set("Target", static_cast<const void *>(garbage));
             }).rfind("refused: parameter Target", 0) == 0,
             "Set refuses a doomed object pointer for an ObjectProperty (control)");
    }
    DestroyWindow(window);
    zsdk::detail::thread::Stop();
    zsdk::Flush();
    return check::Finish("calllend");
}
