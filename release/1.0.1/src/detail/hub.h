#pragma once

#include <cstdint>

namespace zsdk::detail::hub
{
    enum class Role : uint8_t { Off, Owner, Guest };

    using Wake = void (*)();

    inline constexpr uint32_t kLayout = 1;
    inline constexpr uint32_t kSlots = 128;
    inline constexpr uint64_t kQuietMs = 2000;
    inline constexpr uint64_t kMoveMs = 3000;

    Role Join(uint32_t thread, uint32_t message, Wake pump, Wake quit, void *self);
    bool Rehook(void *self, uint64_t now);
    void Leave(uint32_t message);
    int Members();
    uint64_t Moves();
    bool Allowed();
#if defined(ZSDK_TESTS)
    void Deny(bool off);
#endif
}
