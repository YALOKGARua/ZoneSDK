#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include <windows.h>

#include "../src/detail/runtime.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using check::That;

    int *volatile nowhere = nullptr;
    volatile int sink = 0;
    volatile int depthCap = 0x7FFFFFFF;

    struct Sour
    {
        ~Sour()
        {
            *nowhere = 2;
        }
    };

    void Quiet() noexcept
    {
        *nowhere = 3;
    }

    struct Weight
    {
        volatile char *pad;
        ~Weight()
        {
            char room[4096];
            std::snprintf(room, sizeof(room), "%d %d", static_cast<int>(pad[0]), sink);
            sink = sink + room[0] + room[sizeof(room) / 2];
        }
    };

    __declspec(noinline) int Heavy(int depth)
    {
        volatile char pad[512];
        pad[0] = static_cast<char>(depth);
        Weight weight{ pad };
        if (depth > depthCap) return pad[0];
        return Heavy(depth + 1) + pad[0];
    }

    struct Light
    {
        volatile char *pad;
        ~Light()
        {
            pad[1] = pad[0];
        }
    };

    __declspec(noinline) int Lightly(int depth)
    {
        volatile char pad[512];
        pad[0] = static_cast<char>(depth);
        Light light{ pad };
        if (depth > depthCap) return pad[0];
        return Lightly(depth + 1) + pad[0];
    }

    int Child(int which)
    {
        unsigned got = 1;
        std::thread runner([which, &got] {
            if (which == 1)
            {
                ULONG spare = 64 * 1024;
                SetThreadStackGuarantee(&spare);
                got = zsdk::detail::Fend([] { sink = Heavy(0); }).code;
            }
            else got = zsdk::detail::Fend([] { sink = Lightly(0); }).code;
        });
        runner.join();
        return got == EXCEPTION_STACK_OVERFLOW ? 0 : 2;
    }

    DWORD Spawn(int which)
    {
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        std::wstring line = L"\"" + std::wstring(self) + L"\" child " + std::to_wstring(which);
        STARTUPINFOW start{};
        start.cb = sizeof(start);
        PROCESS_INFORMATION proc{};
        const UINT was = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        const bool ran = CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &proc) != 0;
        SetErrorMode(was);
        if (!ran) return 0xFFFFFFFFu;
        DWORD code = 0xFFFFFFFEu;
        if (WaitForSingleObject(proc.hProcess, 60000) == WAIT_OBJECT_0) GetExitCodeProcess(proc.hProcess, &code);
        else TerminateProcess(proc.hProcess, 0xFFFFFFFDu);
        CloseHandle(proc.hThread);
        CloseHandle(proc.hProcess);
        return code;
    }

    void Codes()
    {
        const zsdk::detail::Fended forged = zsdk::detail::Fend([] { RaiseException(zsdk::detail::kCppException, 0, 0, nullptr); });
        That(forged.code == 0 && forged.thrown && forged.what == "unknown exception", "a raised 0xE06D7363 with no C++ object passes the filter and lands in the catch-all, no lane goes off for it");

        const zsdk::detail::Fended trap = zsdk::detail::Fend([] { __debugbreak(); });
        That(trap.code == EXCEPTION_BREAKPOINT && !trap.thrown, "a breakpoint in a hook stops at the fence");

        using Fn = void (*)();
        static Fn volatile target = nullptr;
        const zsdk::detail::Fended jump = zsdk::detail::Fend([] { target(); });
        const std::string named = zsdk::detail::Where(jump.at, jump.from);
        That(jump.code == EXCEPTION_ACCESS_VIOLATION && jump.at == nullptr && zsdk::detail::Where(jump.at) == "an unknown address", "a call through a null function pointer stops at the fence, its own address reads as unknown");
        That(jump.from != nullptr && named.rfind("a call through a null pointer from zsdk-fenceedge.exe+0x", 0) == 0, "the fault line of a null call names the module offset it was called from: " + named);
        const zsdk::detail::Fended plain = zsdk::detail::Fend([] { *nowhere = 7; });
        That(plain.at != nullptr && plain.from == nullptr && zsdk::detail::Where(plain.at, plain.from).rfind("zsdk-fenceedge.exe+0x", 0) == 0, "a fault at a known address keeps its own module offset: " + zsdk::detail::Where(plain.at, plain.from));

        const zsdk::detail::Fended stern = zsdk::detail::Fend([] { RaiseException(0xC0000025u, EXCEPTION_NONCONTINUABLE, 0, nullptr); });
        That(stern.code == 0xC0000025u, "a non-continuable structured exception stops at the fence");
    }

    void Nesting()
    {
        zsdk::detail::Fended inner;
        const zsdk::detail::Fended outer = zsdk::detail::Fend([&inner] { inner = zsdk::detail::Fend([] { *nowhere = 1; }); });
        That(inner.code == EXCEPTION_ACCESS_VIOLATION && outer.code == 0 && !outer.thrown, "a fence inside a fence keeps the fault to the inner one");

        const zsdk::detail::Fended tail = zsdk::detail::Fend([] {
            Sour sour;
            sink = 1;
        });
        That(tail.code == EXCEPTION_ACCESS_VIOLATION, "an access violation in a destructor at the end of a scope stops at the fence");

        const zsdk::detail::Fended hushed = zsdk::detail::Fend([] { Quiet(); });
        That(hushed.code == EXCEPTION_ACCESS_VIOLATION, "an access violation in a noexcept function stops at the fence");

        const zsdk::detail::Fended twice = zsdk::detail::Fend([] {
            Sour sour;
            *nowhere = 1;
        });
        That(twice.code == EXCEPTION_ACCESS_VIOLATION, "a destructor that faults while the first fault unwinds is fenced too");
    }

    void Crowd()
    {
        std::atomic<int> caught{ 0 };
        std::vector<std::thread> crowd;
        for (int t = 0; t < 8; ++t)
            crowd.emplace_back([&caught] {
                for (int i = 0; i < 2000; ++i)
                    if (zsdk::detail::Fend([] { *nowhere = 5; }).code == EXCEPTION_ACCESS_VIOLATION) ++caught;
            });
        for (std::thread &t : crowd) t.join();
        That(caught.load() == 16000, "8 threads that fault 2000 times each at once are all fenced: " + std::to_string(caught.load()));
    }

    void Cost()
    {
        constexpr int kCalls = 1000000;
        volatile int counter = 0;
        std::vector<double> runs;
        for (int k = 0; k < 3; ++k)
        {
            const auto from = std::chrono::steady_clock::now();
            for (int i = 0; i < kCalls; ++i) (void)zsdk::detail::Fend([&counter] { counter = counter + 1; });
            runs.push_back(std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - from).count() / kCalls);
        }
        std::sort(runs.begin(), runs.end());
        std::printf("fenceedge: a clean fenced call costs %.1f ns (median of 3 x 1M)\n", runs[1]);
        That(runs[1] < 200.0, "a clean fenced call stays under 200 ns, so a pipeline bind hook fenced per call is cheap: " + std::to_string(runs[1]) + " ns");
    }

    void Overflows()
    {
        const DWORD guarded = Spawn(1);
        That(guarded == 0, "with a 64 KB stack guarantee an overflow whose frames run a 4 KB destructor during the unwind stops at the fence: exit " + std::to_string(guarded));
        const DWORD light = Spawn(2);
        That(light == 0, "an overflow through frames with a small destructor each stops at the fence: exit " + std::to_string(light));
    }
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 3 && std::wstring(argv[1]) == L"child") return Child(_wtoi(argv[2]));
    Codes();
    Nesting();
    Crowd();
    Cost();
    Overflows();
    return check::Finish("fenceedge");
}
