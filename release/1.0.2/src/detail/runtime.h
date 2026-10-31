#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace zsdk
{
    class Input;

    namespace ue
    {
        struct Param;
    }

    namespace world
    {
        struct Grip;
    }
}

namespace zsdk::detail
{
    void Gate(const std::string &key, bool marked);
    void Halt();
    bool Halted();
    void Exit(bool on);
    bool Exiting();
#if defined(ZSDK_TESTS)
    void Grip(int which, bool hold);
#endif

    inline constexpr unsigned kCppException = 0xE06D7363u;

    struct Fended
    {
        unsigned code = 0;
        const void *at = nullptr;
        const void *from = nullptr;
        bool thrown = false;
        std::string what;
    };

    inline constexpr unsigned long kFenceSpare = 32 * 1024;
    inline constexpr uintptr_t kFenceStackFloor = 1024 * 1024;

    Fended Fend(void (*work)(void *), void *context);
    std::string Where(const void *at);
    std::string Where(const void *at, const void *from);

    template<class F> Fended Fend(F &&work)
    {
        using Work = std::remove_reference_t<F>;
        return Fend([](void *context) { (*static_cast<Work *>(context))(); }, const_cast<void *>(static_cast<const void *>(&work)));
    }

    namespace call
    {
        bool Fits(const void *on, const void *owner, const void *function);
        void Aim(const std::string &key, const void *on, bool checked, const void *owner, const void *function);
        void Pass(const std::string &key, const ue::Param &param, const void *source, size_t bytes);
    }

    namespace input
    {
        inline constexpr uint64_t kSteadyMs = 400;

        struct Glance
        {
            bool on = false;
            uint64_t since = 0;
            bool stirred = false;
            uint64_t stirredAt = 0;
        };

        bool Steady(Glance &glance, bool cursor, bool stirred, uint64_t now);
        Input Frame(void *runtime);
    }

    namespace census
    {
        void Turn(uint32_t epoch);
        void Tally(uint64_t &sweptAt, uint32_t &sweptEpoch);
    }

    namespace world
    {
        void Watch(uint64_t now);
        bool Advance();
        void Seen();
        zsdk::world::Grip &Spare();
    }

    namespace thread
    {
        enum class Wired : uint8_t { Blocked, Refused, Failed, Done, Idle, Stale, Spent };

        inline constexpr uint64_t kLostMs = 2000;
        inline constexpr uint64_t kDrawnMs = 500;

        struct Beat
        {
            bool hooked = false;
            uint64_t pumpAt = 0;
            uint64_t presentAt = 0;
            uint64_t excusedAt = 0;
        };

        bool Lost(const Beat &beat, uint64_t now);
        bool Gap(uint64_t before, uint64_t now);

        void Start();
        void Stop();
        void Present();
        void Overlay(bool open);
        void Menu(bool open);
        Wired WireStep(bool (*wire)());
        Wired Wiring(uint64_t now, bool (*wire)());
        bool WireBlocked();
        uint64_t Turns();
        bool Watching();
    }

    namespace toast
    {
        void Focus(bool focused);
    }

    namespace job
    {
        void Seal();
        void Run(uint64_t now);
        void Stuck(uint64_t now);
    }
}

#if defined(ZSDK_TESTS)
namespace zsdk::detail::world
{
    void Jolt(const char *why, uint64_t now);
    void Latch(bool hold);
}

namespace zsdk::detail::census
{
    void Latch(bool hold);
}
#endif

#if defined(ZSDK_TESTS)
namespace zsdk::detail
{
    void Pretend(const wchar_t *file);
    void ReportFolder(const wchar_t *folder);
}
#endif

#if defined(ZSDK_TESTS)
namespace zsdk::detail::thread
{
    void Cell(const uint8_t *cell);
}
#endif
