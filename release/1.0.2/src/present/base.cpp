#include <zsdk/present.h>

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <iterator>

namespace zsdk
{
    double Ms()
    {
        static LARGE_INTEGER rate{};
        if (!rate.QuadPart) QueryPerformanceFrequency(&rate);
        LARGE_INTEGER at{};
        QueryPerformanceCounter(&at);
        return static_cast<double>(at.QuadPart) * 1000.0 / static_cast<double>(rate.QuadPart);
    }

    uint64_t Now()
    {
        return GetTickCount64();
    }

    namespace
    {
        std::wstring Widen(const char *ascii)
        {
            std::wstring out;
            for (const char *walk = ascii ? ascii : ""; *walk; ++walk) out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
            return out;
        }

        void Add(std::string &out, bool differs, const char *text)
        {
            if (!differs) return;
            if (!out.empty()) out += ", ";
            out += text;
        }
    }

    std::wstring WakeName(const Identity &identity)
    {
        return Widen(identity.tag) + L".GameThread.1";
    }

    std::wstring MutexShape(const Identity &identity)
    {
        return L"Local\\" + Widen(identity.tag) + L".Addon.%lu";
    }

    bool Legacy(const Policy &policy)
    {
        Policy knobs = policy;
        knobs.watchKept = Policy{}.watchKept;
        return !(knobs == Policy{});
    }

    std::string LegacyText(const Policy &p)
    {
        const Policy d{};
        std::string out;
        char knock[64]{};
        _snprintf_s(knock, sizeof(knock), _TRUNCATE, "knock=%s %u/%u", p.knock == Knock::BusyIdle ? "busyidle" : p.knock == Knock::Fixed ? "fixed" : "configured", p.knockMs, p.idleMs);
        char wire[48]{};
        _snprintf_s(wire, sizeof(wire), _TRUNCATE, "wire=%s %u", p.wire == WireRetry::Backoff ? "backoff" : "fixed", p.wireMs);
        Add(out, p.marks != d.marks, p.marks == Marks::All ? "marks=all" : "marks=flagged");
        Add(out, p.strictMarks != d.strictMarks, p.strictMarks ? "strictMarks=1" : "strictMarks=0");
        Add(out, p.verifiedOnly != d.verifiedOnly, p.verifiedOnly ? "verifiedOnly=1" : "verifiedOnly=0");
        Add(out, p.halt != d.halt, p.halt == Halt::Key ? "halt=key" : "halt=session");
        Add(out, p.haltOnSeh != d.haltOnSeh, p.haltOnSeh ? "haltOnSeh=1" : "haltOnSeh=0");
        Add(out, p.gone != d.gone, p.gone == GoneFlags::Strict ? "gone=strict" : "gone=legacy");
        Add(out, p.spelling != d.spelling, p.spelling == Spelling::Tolerant ? "spelling=tolerant" : "spelling=strict");
        Add(out, p.widen != d.widen, p.widen ? "widen=1" : "widen=0");
        Add(out, p.census != d.census, p.census == CensusMode::Interval ? "census=interval" : "census=ondemand");
        Add(out, p.knock != d.knock || p.knockMs != d.knockMs || p.idleMs != d.idleMs, knock);
        Add(out, p.wire != d.wire || p.wireMs != d.wireMs, wire);
        Add(out, p.mods != d.mods, p.mods == ModRule::Alt ? "mods=alt" : "mods=exact");
        return out.empty() ? std::string("none") : out;
    }

    namespace registry
    {
        namespace
        {
            constexpr Row kRows[] = {
                { "ride", "ZoneRide", 0.12f, "G" },
                { "heli", "ZoneHeli", 0.18f, "CAPSLOCK|9" },
                { "balloon", "ZoneBalloon", 0.24f, "0|9|ENTER" },
                { "mount", "ZoneMount", 0.30f, "-|9|[" },
                { "skif", "ZoneSkif", 0.36f, "]|\\|;" },
                { "zipline", "ZoneZipline", 0.42f, "'|9" },
                { "grenades", "ZoneGrenades", 0.48f, ".|," },
                { "underground", "ZoneUnderground", 0.12f, "F7" },
                { "limansk", "ZoneLimansk", 0.16f, "ALT+9" },
                { "mercy", "Mercy", 0.12f, "" },
                { "spawner", "Spawner", 0.0f, "" },
                { "pulse", "ZonePulse", 0.60f, "ALT+SHIFT+J|ALT+J|ALT+K" },
                { "coop", "ZoneCoop", 0.0f, "" },
                { "newland", "ZoneNewLand", 0.0f, "" },
                { "cheats", "ZoneCheats", 0.0f, "" },
                { "blinddog", "ZoneBlindDog", 0.0f, "" },
                { "wounds", "ZoneWounds", 0.0f, "" },
                { "template", "ZoneTemplate", 0.54f, "ALT+L" },
            };
        }

        std::span<const Row> Rows()
        {
            return kRows;
        }

        const Row *Find(std::string_view id)
        {
            for (const Row &row : kRows)
                if (id == row.id) return &row;
            return nullptr;
        }

        float Lane(std::string_view id, float fallback)
        {
            const Row *row = Find(id);
            return row && row->lane > 0.0f ? row->lane : fallback;
        }
    }

    namespace app
    {
        const registry::Row *Row()
        {
            const App &a = Get();
            if (a.row.id && a.row.tag) return &a.row;
            return registry::Find(a.identity.registry ? a.identity.registry : "");
        }

        float Lane()
        {
            static std::atomic<bool> told{ false };
            const registry::Row *row = Row();
            if (row) return row->lane > 0.0f && row->lane < 1.0f ? row->lane : 0.0f;
            if (!told.exchange(true))
                Log("toasts: %s has no registry row, its toasts use the spare lane %.2f; give App::row a lane of its own, or 0 for no toasts", Get().identity.name ? Get().identity.name : "the add-on",
                    static_cast<double>(registry::kSpareLane));
            return registry::kSpareLane;
        }
    }

    namespace glyphs
    {
        std::string Undrawn(std::string_view utf8)
        {
            std::string bad;
            for (const uint32_t point : text::Points(utf8))
            {
                if (Drawable(point)) continue;
                char hex[16]{};
                _snprintf_s(hex, sizeof(hex), _TRUNCATE, " U+%04X", point);
                bad += hex;
            }
            return bad;
        }
    }
}
