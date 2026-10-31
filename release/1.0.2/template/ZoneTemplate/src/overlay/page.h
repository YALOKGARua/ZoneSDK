#pragma once

#include <zsdk/overlay.h>

#include "../template.h"

namespace ztemplate::page
{
    void Page(zsdk::Overlay &o);
    std::string Words(const zsdk::toast::Item &item, zsdk::lang::Lang ui);
    uint32_t Tone(const zsdk::toast::Item &item);
}
