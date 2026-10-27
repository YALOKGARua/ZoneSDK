#pragma once

#include <zsdk/ledger.h>

#include <cstdint>

namespace zsdk::detail::mem
{
    bool Mark(const ledger::Window &window, uintptr_t at, uint32_t bit);
}
