#pragma once

#include <zsdk/overlay.h>

#include "../hellohud.h"

namespace zhellohud::page
{
    void Page(zsdk::Overlay &o);
    std::string Words(const zsdk::toast::Item &item, zsdk::lang::Lang ui);
    uint32_t Tone(const zsdk::toast::Item &item);
}

namespace zhellohud::corner
{
    void Draw(zsdk::Overlay &o);
}
