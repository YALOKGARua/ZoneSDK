#include <zsdk/present.h>

#include "../detail/hub.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cwchar>
#include <iterator>
#include <string>

namespace zsdk::detail::hub
{
    namespace
    {
        constexpr uint32_t kMagic = 0x4255485Au;
        constexpr UINT kFirstRegistered = 0xC000;

        struct Slot
        {
            uint32_t message;
            uint32_t spare;
            uint64_t pump;
            uint64_t quit;
        };

        struct Table
        {
            uint32_t magic;
            uint32_t layout;
            uint32_t size;
            uint32_t capacity;
            uint32_t count;
            uint32_t thread;
            uint64_t hook;
            uint64_t owner;
            uint64_t born;
            uint64_t beat;
            uint64_t movedAt;
            uint64_t moves;
            Slot slots[kSlots];
        };

        Table *view = nullptr;
        HANDLE mapping = nullptr;
        HANDLE gate = nullptr;
        bool tried = false;
#if defined(ZSDK_TESTS)
        bool denied = false;
#endif

        template<class T> std::atomic_ref<T> A(T &value)
        {
            return std::atomic_ref<T>(value);
        }

        std::wstring Name(const wchar_t *suffix)
        {
            wchar_t text[96]{};
            std::swprintf(text, std::size(text), L"Local\\ZoneSDK.Hub.%u.%lu%s", kLayout, GetCurrentProcessId(), suffix);
            return text;
        }

        bool Open()
        {
            if (view) return true;
            if (tried) return false;
            tried = true;
            gate = CreateMutexW(nullptr, FALSE, Name(L".Gate").c_str());
            if (!gate) return false;
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(sizeof(Table)), Name(L"").c_str());
            if (!mapping) return false;
            void *raw = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Table));
            if (!raw) return false;
            view = static_cast<Table *>(raw);
            return true;
        }

        struct Locked
        {
            bool held = false;

            Locked()
            {
                if (!gate) return;
                const DWORD got = WaitForSingleObject(gate, 1000);
                held = got == WAIT_OBJECT_0 || got == WAIT_ABANDONED;
            }

            ~Locked()
            {
                if (held) ReleaseMutex(gate);
            }

            Locked(const Locked &) = delete;
            Locked &operator=(const Locked &) = delete;
        };

        bool Fit(Table &t)
        {
            if (A(t.magic).load() == 0)
            {
                t.layout = kLayout;
                t.size = static_cast<uint32_t>(sizeof(Table));
                t.capacity = kSlots;
                t.born = GetTickCount64();
                A(t.magic).store(kMagic, std::memory_order_release);
            }
            return A(t.magic).load(std::memory_order_acquire) == kMagic && t.layout == kLayout && t.size == sizeof(Table) && t.capacity == kSlots;
        }

        uint32_t Used(Table &t)
        {
            return std::min(A(t.count).load(std::memory_order_acquire), kSlots);
        }

        int Live(Table &t)
        {
            int live = 0;
            const uint32_t used = Used(t);
            for (uint32_t i = 0; i < used; ++i)
                if (A(t.slots[i].message).load(std::memory_order_acquire) != 0) ++live;
            return live;
        }

        bool Fan(Table &t, bool quit, UINT message)
        {
            const uint32_t used = Used(t);
            for (uint32_t i = 0; i < used; ++i)
            {
                Slot &s = t.slots[i];
                const uint32_t m = A(s.message).load(std::memory_order_acquire);
                if (m == 0 || (!quit && m != message)) continue;
                const Wake wake = reinterpret_cast<Wake>(static_cast<uintptr_t>(A(quit ? s.quit : s.pump).load(std::memory_order_acquire)));
                if (!quit) A(t.beat).store(GetTickCount64(), std::memory_order_release);
                if (wake) wake();
                if (!quit) return true;
            }
            return false;
        }

        LRESULT CALLBACK Shared(int code, WPARAM wparam, LPARAM lparam)
        {
            Table *t = view;
            if (t && code == HC_ACTION && (wparam & PM_REMOVE) != 0 && lparam)
            {
                MSG &msg = *reinterpret_cast<MSG *>(lparam);
                if (msg.message == WM_QUIT) Fan(*t, true, 0);
                else if (msg.hwnd == nullptr && msg.message >= kFirstRegistered && Fan(*t, false, msg.message)) msg.message = WM_NULL;
            }
            return CallNextHookEx(nullptr, code, wparam, lparam);
        }

        HHOOK Install(void *self, DWORD thread)
        {
            HHOOK made = SetWindowsHookExW(WH_GETMESSAGE, Shared, static_cast<HMODULE>(self), thread);
            if (!made) made = SetWindowsHookExW(WH_GETMESSAGE, Shared, nullptr, thread);
            return made;
        }

        void Drop(Table &t)
        {
            const uint64_t old = A(t.hook).exchange(0);
            if (old) UnhookWindowsHookEx(reinterpret_cast<HHOOK>(static_cast<uintptr_t>(old)));
            A(t.owner).store(0);
            A(t.thread).store(0);
        }
    }

    bool Allowed()
    {
#if defined(ZSDK_TESTS)
        if (denied) return false;
#endif
        wchar_t text[8]{};
        const DWORD got = GetEnvironmentVariableW(L"ZSDK_SHARED_HOOK", text, static_cast<DWORD>(std::size(text)));
        return !(got == 1 && text[0] == L'0');
    }

    Role Join(uint32_t thread, uint32_t message, Wake pump, Wake quit, void *self)
    {
        if (!Allowed() || thread == 0 || message < kFirstRegistered || !pump || !Open()) return Role::Off;
        Locked hold;
        if (!hold.held) return Role::Off;
        Table &t = *view;
        if (!Fit(t)) return Role::Off;
        const uint32_t watched = A(t.thread).load();
        if (watched != 0 && watched != thread)
        {
            if (Live(t) > 0) return Role::Off;
            Drop(t);
        }
        const uint32_t used = Used(t);
        Slot *slot = nullptr;
        for (uint32_t i = 0; i < used && !slot; ++i)
            if (A(t.slots[i].message).load() == 0) slot = &t.slots[i];
        const bool fresh = !slot;
        if (fresh)
        {
            if (used >= kSlots) return Role::Off;
            slot = &t.slots[used];
        }
        A(slot->pump).store(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pump)), std::memory_order_release);
        A(slot->quit).store(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(quit)), std::memory_order_release);
        A(slot->message).store(message, std::memory_order_release);
        if (fresh) A(t.count).store(used + 1, std::memory_order_release);
        if (A(t.hook).load() != 0) return Role::Guest;
        const HHOOK made = Install(self, thread);
        if (!made)
        {
            A(slot->message).store(0, std::memory_order_release);
            return Role::Off;
        }
        A(t.hook).store(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(made)));
        A(t.owner).store(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(self)));
        A(t.thread).store(thread);
        A(t.movedAt).store(GetTickCount64());
        return Role::Owner;
    }

    bool Rehook(void *self, uint64_t now)
    {
        if (!view) return false;
        Locked hold;
        if (!hold.held) return false;
        Table &t = *view;
        const uint32_t thread = A(t.thread).load();
        if (thread == 0 || A(t.hook).load() == 0 || Live(t) == 0) return false;
        const uint64_t beat = std::max(A(t.beat).load(), A(t.born).load());
        if (Age(now, beat) < kQuietMs || Age(now, A(t.movedAt).load()) < kMoveMs) return false;
        const HHOOK made = Install(self, thread);
        if (!made) return false;
        const uint64_t old = A(t.hook).exchange(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(made)));
        A(t.owner).store(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(self)));
        A(t.movedAt).store(now);
        A(t.moves).fetch_add(1);
        if (old) UnhookWindowsHookEx(reinterpret_cast<HHOOK>(static_cast<uintptr_t>(old)));
        return true;
    }

    void Leave(uint32_t message)
    {
        if (!view || message == 0) return;
        Locked hold;
        Table &t = *view;
        const uint32_t used = Used(t);
        for (uint32_t i = 0; i < used; ++i)
            if (A(t.slots[i].message).load() == message) A(t.slots[i].message).store(0, std::memory_order_release);
    }

    int Members()
    {
        return view ? Live(*view) : 0;
    }

    uint64_t Moves()
    {
        return view ? A(view->moves).load() : 0;
    }

#if defined(ZSDK_TESTS)
    void Deny(bool off)
    {
        denied = off;
    }
#endif
}
