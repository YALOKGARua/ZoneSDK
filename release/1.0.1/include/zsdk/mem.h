#pragma once

#include "guard.h"
#include "ledger.h"
#include "ue.h"

#include <cstddef>
#include <cstdint>

namespace zsdk::mem
{
    bool Writable(const ledger::Window &window);
    bool Put(const ledger::Window &window, uintptr_t at, const void *from, size_t bytes);
    bool Poke(void *at, const void *from, size_t bytes);
    int Pokes();

    template<class T> bool Write(const ledger::Window &window, uintptr_t at, const T &value)
    {
        return Put(window, at, &value, sizeof(T));
    }

    template<class T> bool Write(const ledger::Window &window, uintptr_t base, const guard::Field &field, const T &value)
    {
        uintptr_t at = 0;
        return base >= 0x10000 && guard::Use(field, at) && Put(window, base + at, &value, sizeof(T));
    }

    template<class T> bool Read(uintptr_t base, const guard::Field &field, T &out)
    {
        uintptr_t at = 0;
        return base >= 0x10000 && guard::Use(field, at) && ue::Read(base + at, out);
    }
}
