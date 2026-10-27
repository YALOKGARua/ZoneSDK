#pragma once

#include <windows.h>

#include "app.h"
#include "hooks.h"

namespace zsdk::shell
{
    BOOL Main(HMODULE module, DWORD reason, LPVOID reserved, const Hooks &hooks);
    void *Module();
    bool Stopping();
    bool Ready();
    bool Exiting();
}

#define ZSDK_ADDON(name, description) \
    extern "C" __declspec(dllexport) const char *NAME = name; \
    extern "C" __declspec(dllexport) const char *DESCRIPTION = description;
