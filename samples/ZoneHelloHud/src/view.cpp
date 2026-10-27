#include <zsdk/present.h>

#include "hellohud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zhellohud::view
{
    namespace
    {
        constexpr const char *kPoints[] = { "dir.n", "dir.ne", "dir.e", "dir.se", "dir.s", "dir.sw", "dir.w", "dir.nw" };

        double Round360(double degrees)
        {
            const double turned = std::fmod(degrees, 360.0);
            const double out = turned < 0.0 ? turned + 360.0 : turned;
            return out >= 360.0 ? out - 360.0 : out;
        }

        const char *Tr(std::string_view id, zsdk::lang::Lang ui)
        {
            return zsdk::lang::Text(id, ui);
        }
    }

    const char *PhaseId(zsdk::Phase phase)
    {
        switch (phase)
        {
        case zsdk::Phase::Hooking: return "phase.hooking";
        case zsdk::Phase::Binding: return "phase.binding";
        case zsdk::Phase::Wiring: return "phase.wiring";
        case zsdk::Phase::Waiting: return "phase.waiting";
        case zsdk::Phase::Ready: return "phase.ready";
        case zsdk::Phase::Failed: return "phase.failed";
        case zsdk::Phase::Halted: return "phase.halted";
        case zsdk::Phase::Lost: return "phase.lost";
        default: return "phase.off";
        }
    }

    std::string State(const Status &st, zsdk::lang::Lang ui)
    {
        std::string state = Tr(PhaseId(st.phase), ui);
        if (st.phase == zsdk::Phase::Waiting && zsdk::lang::Has(st.wait)) state += std::string(". ") + Tr(st.wait, ui);
        return state;
    }

    double Bearing(double yaw)
    {
        return Round360(yaw + 90.0);
    }

    const char *Point(double bearing)
    {
        const int sector = static_cast<int>(std::floor(Round360(bearing) / 45.0 + 0.5)) % 8;
        return kPoints[sector];
    }

    double Pace(const zsdk::math::Vec3 &velocity, Unit unit)
    {
        const double flat = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        return unit == Unit::Ms ? flat / 100.0 : flat * 0.036;
    }

    std::string Fixed(double value, int digits)
    {
        const double scale = std::pow(10.0, digits);
        const double rounded = std::fabs(value) * scale < 0.5 ? 0.0 : std::round(value * scale) / scale;
        char text[48]{};
        std::snprintf(text, sizeof(text), "%.*f", digits, rounded);
        return text;
    }

    bool Fresh(const Status &st, uint64_t now)
    {
        return st.live && st.reading.when != 0 && zsdk::Age(now, st.reading.when) <= kStaleMs;
    }

    std::vector<std::string> Lines(const Status &st, Unit unit, uint64_t now, zsdk::lang::Lang ui)
    {
        std::vector<std::string> out;
        if (!st.live)
        {
            out.push_back(Tr("say.hidden", ui));
            return out;
        }
        if (!Fresh(st, now))
        {
            std::string why = Tr("hud.reading", ui);
            if (st.phase != zsdk::Phase::Ready) why = State(st, ui);
            else if (!st.refusal.empty() && zsdk::lang::Has(st.refusal)) why = Tr(st.refusal, ui);
            out.push_back(zsdk::lang::Format(Tr("hud.wait", ui), { why }));
            return out;
        }
        const Reading &r = st.reading;
        out.push_back(zsdk::lang::Format(Tr("hud.where", ui), { Fixed(r.at.x / 100.0, 1), Fixed(r.at.y / 100.0, 1), Fixed(r.at.z / 100.0, 1) }));
        if (r.turned)
        {
            const double bearing = Bearing(r.rot.yaw);
            out.push_back(zsdk::lang::Format(Tr("hud.heading", ui), { std::to_string(static_cast<int>(std::lround(bearing)) % 360), Tr(Point(bearing), ui) }));
        }
        if (r.moving) out.push_back(zsdk::lang::Format(Tr(unit == Unit::Ms ? "hud.ms" : "hud.kmh", ui), { Fixed(Pace(r.velocity, unit), 1) }));
        return out;
    }

    Spot Place(Corner corner, float wide, float tall, float boxWide, float boxTall, float edge)
    {
        const bool right = corner == Corner::TopRight || corner == Corner::BottomRight;
        const bool bottom = corner == Corner::BottomLeft || corner == Corner::BottomRight;
        const float x = right ? wide - edge - boxWide : edge;
        const float y = bottom ? tall - edge - boxTall : edge;
        return Spot{ .x = std::floor(std::max(0.0f, x)), .y = std::floor(std::max(0.0f, y)) };
    }
}
