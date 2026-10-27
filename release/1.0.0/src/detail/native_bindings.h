#pragma once

#include <cstddef>
#include <cstdint>

namespace zsdk::detail::native_bindings
{
    enum class Status : std::uint8_t { Pending, Scanning, Ready, Rejected };
    bool Initialize() noexcept;
    bool Ready() noexcept;
    Status State() noexcept;
    std::uintptr_t Address(std::uint32_t baselineRva) noexcept;
    std::uintptr_t Base() noexcept;
    std::size_t ImageSize() noexcept;
}
