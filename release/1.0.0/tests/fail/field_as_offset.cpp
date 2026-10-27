#include <zsdk/guard.h>

namespace
{
    constexpr zsdk::guard::Guard kList[] = { { 0x68F67DB, "48 89 9F 40 03 00 00" } };
    constexpr zsdk::guard::Set kSet{ "probe", &zsdk::guard::k61bc1e03, kList, 1, zsdk::guard::Need::Bytes };
    constexpr zsdk::guard::Field kThrower{ "Probe.Thrower", 0x340, kSet, 0, 3 };
}

uintptr_t Probe(uintptr_t base)
{
    return base + kThrower;
}