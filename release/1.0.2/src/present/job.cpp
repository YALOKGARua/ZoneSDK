#include <zsdk/present.h>

#include "../detail/ledger_raw.h"
#include "../detail/runtime.h"

#include <windows.h>

#include <malloc.h>

#include <atomic>
#include <mutex>
#include <string>

namespace zsdk::detail
{
    namespace
    {
        constexpr size_t kSaidMost = 1024;

        const void *Caller(const CONTEXT *context)
        {
            if (!context || !context->Rsp || (context->Rsp & 7u) != 0) return nullptr;
            ULONG_PTR low = 0;
            ULONG_PTR high = 0;
            GetCurrentThreadStackLimits(&low, &high);
            if (context->Rsp < low || context->Rsp + sizeof(void *) > high) return nullptr;
            return *reinterpret_cast<const void *const *>(context->Rsp);
        }

        int Sift(const EXCEPTION_POINTERS *info, const void **at, const void **from)
        {
            const EXCEPTION_RECORD *record = info ? info->ExceptionRecord : nullptr;
            if (!record || record->ExceptionCode == kCppException) return EXCEPTION_CONTINUE_SEARCH;
            *at = record->ExceptionAddress;
            if (!*at) *from = Caller(info->ContextRecord);
            return EXCEPTION_EXECUTE_HANDLER;
        }

        unsigned Walled(void (*work)(void *), void *context, const void **at, const void **from)
        {
            __try
            {
                work(context);
                return 0;
            }
            __except (Sift(GetExceptionInformation(), at, from))
            {
                const unsigned code = GetExceptionCode();
                return code ? code : 1u;
            }
        }

        size_t Spell(const std::exception *error, char *room, size_t size)
        {
            __try
            {
                const char *text = error->what();
                size_t n = 0;
                while (text && n + 1 < size && text[n])
                {
                    room[n] = text[n];
                    ++n;
                }
                room[n] = '\0';
                return n;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                room[0] = '\0';
                return size;
            }
        }

        std::string Said(const std::exception &error)
        {
            char room[kSaidMost];
            if (Spell(&error, room, sizeof(room)) == sizeof(room)) return "an exception whose text cannot be read";
            return room;
        }

        void Spare()
        {
            static thread_local bool spared = false;
            if (spared) return;
            spared = true;
            ULONG_PTR low = 0;
            ULONG_PTR high = 0;
            GetCurrentThreadStackLimits(&low, &high);
            if (high - low < kFenceStackFloor) return;
            ULONG now = 0;
            if (!SetThreadStackGuarantee(&now) || now >= kFenceSpare) return;
            ULONG want = kFenceSpare;
            SetThreadStackGuarantee(&want);
        }
    }

    Fended Fend(void (*work)(void *), void *context)
    {
        Spare();
        Fended out;
        try
        {
            out.code = Walled(work, context, &out.at, &out.from);
        }
        catch (const std::exception &error)
        {
            out.thrown = true;
            out.what = Said(error);
        }
        catch (...)
        {
            out.thrown = true;
            out.what = "unknown exception";
        }
        if (out.code == EXCEPTION_STACK_OVERFLOW) _resetstkoflw();
        return out;
    }

    std::string Where(const void *at, const void *from)
    {
        if (at || !from) return Where(at);
        return "a call through a null pointer from " + Where(from);
    }

    std::string Where(const void *at)
    {
        if (!at) return "an unknown address";
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCWSTR>(at), &module) || !module)
            return "an address outside every module";
        wchar_t path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        const wchar_t *name = path;
        for (DWORD i = 0; i < length; ++i)
            if (path[i] == L'\\' || path[i] == L'/') name = path + i + 1;
        char narrow[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, name, -1, narrow, MAX_PATH, nullptr, nullptr);
        char out[MAX_PATH + 32]{};
        _snprintf_s(out, sizeof(out), _TRUNCATE, "%s+0x%llX", narrow[0] ? narrow : "module", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(at) - reinterpret_cast<uintptr_t>(module)));
        return out;
    }
}

namespace zsdk::job
{
    namespace
    {
        constexpr size_t kLongestName = 32;

        struct Slot
        {
            std::string name;
            std::string key;
            uint32_t period = 0;
            void (*run)(uint64_t now) = nullptr;
            uint64_t last = 0;
            uint64_t runs = 0;
            bool blocked = false;
            std::atomic<bool> off{ false };
        };

        std::mutex gate;
        Slot slots[kMost];
        int count = 0;
        int turn = 0;
        std::atomic<bool> sealed{ false };
        std::atomic<int> running{ -1 };
        std::atomic<uint64_t> runningAt{ 0 };
        std::atomic<uint64_t> toldAt{ 0 };

        bool Named(const char *name)
        {
            if (!name || !*name) return false;
            size_t length = 0;
            for (const char *walk = name; *walk; ++walk)
            {
                const char c = *walk;
                const bool fine = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
                if (!fine || ++length > kLongestName) return false;
            }
            return true;
        }

        bool Refuse(const char *name, const char *why)
        {
            Log("job %s refused: %s", name && *name ? name : "-", why);
            return false;
        }

        void One(int index, uint64_t now)
        {
            Slot &slot = slots[index];
            slot.last = now;
            ++slot.runs;
            const double from = Ms();
            runningAt = GetTickCount64();
            running = index;
            const detail::Fended fended = detail::Fend([&slot, now] { slot.run(now); });
            running = -1;
            const double ms = Ms() - from;
            if (fended.thrown) Rare(("job error " + slot.name).c_str(), "job %s error: %s", slot.name.c_str(), fended.what.c_str());
            if (fended.code)
            {
                slot.off = true;
                Rare(("job seh " + slot.name).c_str(), "job %s: structured exception 0x%08X, the job is off for this session and %s is disabled until Try again", slot.name.c_str(), fended.code, slot.key.c_str());
                ledger::raw::Disable(slot.key);
                return;
            }
            if (ms > kStallMs)
            {
                slot.off = true;
                Rare(("job stall " + slot.name).c_str(), "job %s ran %.0f ms, over the %u ms one run may hold the SDK worker, the job is off for this session", slot.name.c_str(), ms, kStallMs);
                return;
            }
            if (ms > kSlowMs) Rare(("job slow " + slot.name).c_str(), "job %s ran %.0f ms, a job returns within %u ms and polls job::Stopping()", slot.name.c_str(), ms, kSlowMs);
        }
    }

    bool Every(const char *name, uint32_t periodMs, void (*run)(uint64_t now))
    {
        std::lock_guard<std::mutex> hold(gate);
        if (sealed.load() || thread::Started()) return Refuse(name, "jobs are registered from hooks.boot, before the SDK worker starts");
        if (!Named(name)) return Refuse(name, "a job name is 1 to 32 letters, digits, '.', '_' or '-'");
        if (!run) return Refuse(name, "no function");
        if (periodMs < kShortestMs) return Refuse(name, "the period is below 100 ms");
        for (int i = 0; i < count; ++i)
            if (slots[i].name == name) return Refuse(name, "the name is taken");
        if (count >= kMost) return Refuse(name, "an add-on has at most 8 jobs");
        Slot &slot = slots[count++];
        slot.name = name;
        slot.key = std::string("job.") + name;
        slot.period = periodMs;
        slot.run = run;
        Log("job %s every %u ms", name, periodMs);
        return true;
    }

    bool Stopping()
    {
        return thread::Stopping() || detail::Exiting();
    }

    bool Off(const char *name)
    {
        if (!name) return false;
        std::lock_guard<std::mutex> hold(gate);
        for (int i = 0; i < count; ++i)
            if (slots[i].name == name) return slots[i].off.load() || ledger::Blocked(slots[i].key);
        return false;
    }
}

namespace zsdk::detail::job
{
    void Seal()
    {
        std::lock_guard<std::mutex> hold(zsdk::job::gate);
        zsdk::job::sealed = true;
    }

    void Run(uint64_t now)
    {
        using namespace zsdk::job;
        if (!sealed.load() || count == 0 || zsdk::job::Stopping()) return;
        const double from = Ms();
        for (int k = 0; k < count; ++k)
        {
            const int index = (turn + k) % count;
            Slot &slot = slots[index];
            if (slot.off.load() || (slot.runs && Age(now, slot.last) < slot.period)) continue;
            if (zsdk::job::Stopping()) return;
            if (Ms() - from >= kBudgetMs)
            {
                turn = (turn + k) % count;
                return;
            }
            if (ledger::Blocked(slot.key))
            {
                if (!slot.blocked) Log("job %s is disabled after a crash, Try again in the tab clears it", slot.name.c_str());
                slot.blocked = true;
                continue;
            }
            slot.blocked = false;
            One(index, now);
        }
        turn = (turn + 1) % count;
    }

    void Stuck(uint64_t now)
    {
        using namespace zsdk::job;
        const int index = running.load();
        if (index < 0 || index >= kMost || !sealed.load()) return;
        const uint64_t at = runningAt.load();
        if (!at || Age(now, at) < kStallMs || toldAt.exchange(at) == at) return;
        Log("job %s has not returned for %llu ms, the SDK worker waits for it: no other job, census step or exit poll runs until it returns", slots[index].name.c_str(), static_cast<unsigned long long>(Age(now, at)));
    }
}
