#pragma once

#if defined(IMGUI_VERSION) || defined(IMGUI_VERSION_NUM)
#error ImGui is visible in a present unit; ImGui code belongs in src\overlay only
#endif

#if defined(ZSDK_PRESENT_UNIT) && !defined(IMGUI_DISABLE)
#error present units are compiled with IMGUI_DISABLE
#endif

#include "zsdk.h"
