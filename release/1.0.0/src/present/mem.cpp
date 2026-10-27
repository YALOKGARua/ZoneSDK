#include <zsdk/present.h>

#include "../detail/engine_core.h"
#include "../detail/mem_raw.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <intrin.h>

namespace zsdk::mem
{
    namespace
    {
        std::atomic<int> pokes{ 0 };

        bool Copy(void *at, const void *from, size_t bytes)
        {
            __try
            {
                memcpy(at, from, bytes);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool Or(void *at, uint32_t bit)
        {
            __try
            {
                _InterlockedOr(static_cast<volatile long *>(at), static_cast<long>(bit));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool Here()
        {
            if (zsdk::thread::OnGame() && zsdk::detail::thread::InPump() && !zsdk::thread::Stopping()) return true;
            Rare("memory write off pump", "memory: a game memory write outside the game thread pump was refused");
            return false;
        }
    }

    bool Writable(const ledger::Window &window)
    {
        if (!window.Open() || window.Refused() || window.Key().empty()) return false;
        if (ledger::Blocked(window.Key()))
        {
            Rare(window.Key().c_str(), "memory: the window %s is disabled after a crash, its writes are refused", window.Key().c_str());
            return false;
        }
        return Here();
    }

    bool Poke(void *at, const void *from, size_t bytes)
    {
        if (!at || reinterpret_cast<uintptr_t>(at) < 0x10000 || !from || !bytes || !Here()) return false;
        const bool ok = Copy(at, from, bytes);
        if (ok) ++pokes;
        return ok;
    }

    bool Put(const ledger::Window &window, uintptr_t at, const void *from, size_t bytes)
    {
        return at >= 0x10000 && Writable(window) && Poke(reinterpret_cast<void *>(at), from, bytes);
    }

    int Pokes()
    {
        return pokes.load();
    }
}

namespace zsdk::detail::mem
{
    bool Mark(const ledger::Window &window, uintptr_t at, uint32_t bit)
    {
        return at >= 0x10000 && zsdk::mem::Writable(window) && zsdk::mem::Or(reinterpret_cast<void *>(at), bit);
    }
}
