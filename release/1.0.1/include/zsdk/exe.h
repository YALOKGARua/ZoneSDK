#pragma once

#include "guard.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace zsdk::exe
{
    inline constexpr const wchar_t *kSteamFile = L"Stalker2-Win64-Shipping.exe";

    std::wstring File();
    std::string Name();
    bool Game(std::wstring_view file);
    bool Steam(std::wstring_view file);
    bool Steam();
    uintptr_t Base();
    uint32_t ImageSize();
    uint32_t Stamp();
    std::string Hash();
    std::string Sha();
    std::string Short();
    bool Hashed();
    bool Known();
    const guard::Build *Build();
}
