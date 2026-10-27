#include <zsdk/present.h>

#include <reshade_api.hpp>

#include "../detail/runtime.h"

#include <windows.h>

#include <atomic>

namespace zsdk
{
    struct InputAccess
    {
        static Input Make(void *runtime, bool focused, bool overlaid, bool menu, bool paused)
        {
            Input in;
            in.runtime = runtime;
            in.focused = focused;
            in.overlaid = overlaid;
            in.menu = menu;
            in.paused = paused;
            return in;
        }

        static reshade::api::effect_runtime *Runtime(const Input &in)
        {
            return static_cast<reshade::api::effect_runtime *>(in.runtime);
        }
    };

    namespace
    {
        std::atomic<bool> menuNow{ false };
        detail::input::Glance glance;

        bool ForegroundIsOurs()
        {
            const HWND front = GetForegroundWindow();
            DWORD pid = 0;
            if (front) GetWindowThreadProcessId(front, &pid);
            return pid == GetCurrentProcessId();
        }

        bool CursorShown()
        {
            CURSORINFO info{};
            info.cbSize = sizeof(info);
            return GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING) != 0 && info.hCursor != nullptr;
        }
    }

    bool Input::Down(uint32_t vk) const
    {
        const auto *r = InputAccess::Runtime(*this);
        return r && r->is_key_down(vk);
    }

    bool Input::Pressed(uint32_t vk) const
    {
        const auto *r = InputAccess::Runtime(*this);
        return r && r->is_key_pressed(vk);
    }

    bool Input::Released(uint32_t vk) const
    {
        const auto *r = InputAccess::Runtime(*this);
        return r && r->is_key_released(vk);
    }

    bool Input::MouseDown(int button) const
    {
        const auto *r = InputAccess::Runtime(*this);
        return r && r->is_mouse_button_down(static_cast<uint32_t>(button));
    }

    bool Input::MousePressed(int button) const
    {
        const auto *r = InputAccess::Runtime(*this);
        return r && r->is_mouse_button_pressed(static_cast<uint32_t>(button));
    }

    void Input::Cursor(uint32_t &x, uint32_t &y, int16_t *wheel) const
    {
        x = y = 0;
        if (wheel) *wheel = 0;
        const auto *r = InputAccess::Runtime(*this);
        if (r) r->get_mouse_cursor_position(&x, &y, wheel);
    }

    bool Input::Async(uint32_t vk) const
    {
        return (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
    }

    uint8_t Input::Mods() const
    {
        return keys::Mods(Down(VK_CONTROL), Down(VK_SHIFT), Down(VK_MENU));
    }

    bool Input::Fires(keys::Chord chord) const
    {
        return chord.key && Pressed(chord.key) && keys::Fires(chord, chord.key, Mods());
    }

    bool Input::Focused() const
    {
        return focused;
    }

    bool Input::Overlaid() const
    {
        return overlaid;
    }

    bool Input::Menu() const
    {
        return menu;
    }

    bool Input::Paused() const
    {
        return paused;
    }

    namespace detail::input
    {
        bool Steady(Glance &g, bool cursor, bool stirred, uint64_t now)
        {
            if (stirred)
            {
                g.stirred = true;
                g.stirredAt = now;
            }
            if (!cursor)
            {
                g.on = false;
                return false;
            }
            if (!g.on || now < g.since)
            {
                g.on = true;
                g.since = now;
            }
            if (stirred) return false;
            if (g.stirred && (now < g.stirredAt || now - g.stirredAt < kSteadyMs)) return false;
            return now - g.since >= kSteadyMs;
        }

        Input Frame(void *runtime)
        {
            const bool focused = ForegroundIsOurs();
            const bool open = zsdk::thread::Overlaid();
            const bool paused = zsdk::world::Paused();
            const auto *r = static_cast<reshade::api::effect_runtime *>(runtime);
            const bool mods = r && (r->is_key_down(VK_MENU) || r->is_key_down(VK_CONTROL) || r->is_key_down(VK_SHIFT));
            const bool steady = Steady(glance, focused && CursorShown(), mods || open, GetTickCount64());
            const bool menu = focused && !open && (paused || steady);
            if (menu != menuNow.load()) detail::thread::Menu(menu);
            menuNow = menu;
            detail::toast::Focus(focused);
            return InputAccess::Make(runtime, focused, open, menu, paused);
        }
    }
}
