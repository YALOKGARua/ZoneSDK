#include <zsdk/overlay.h>

#include "page.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace zhellohud::corner
{
    namespace
    {
        namespace paint = zsdk::paint;

        uint64_t revision = 0;
        bool on = false;
        Corner where = Corner::TopLeft;
        Unit unit = Unit::Kmh;
    }

    void Draw(zsdk::Overlay &o)
    {
        if (!o.runtime || !zsdk::overlay::Inside()) return;
        const uint64_t seen = zsdk::ini::Revision();
        if (seen != revision)
        {
            revision = seen;
            const Settings s = zsdk::ini::Config<Settings>();
            on = s.enabled && s.shown;
            where = s.corner;
            unit = s.unit;
        }
        if (!on) return;
        const uint64_t now = zsdk::Now();
        const Status st = core::Snapshot();
        const bool fresh = view::Fresh(st, now);
        if (!zsdk::thread::Overlaid() && !(fresh && frame::Clear())) return;
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        if (screen.x < 320.0f || screen.y < 240.0f) return;
        ImDrawList *sheet = ImGui::GetForegroundDrawList(nullptr);
        if (!sheet) return;
        const std::vector<std::string> lines = view::Lines(st, unit, now, zsdk::overlay::Ui());
        if (lines.empty()) return;
        const float scale = std::clamp(screen.y / 1080.0f, 1.0f, 2.0f);
        const float size = std::round(18.0f * scale);
        const float pad = std::round(8.0f * scale);
        const float gap = std::round(4.0f * scale);
        float widest = 0.0f;
        for (const std::string &line : lines) widest = std::max(widest, paint::Span(size, line.c_str()));
        const float step = size * 2.0f;
        const float wide = std::ceil((widest + pad * 2.0f) / step) * step;
        const float tall = std::ceil(static_cast<float>(lines.size()) * (size + gap) - gap + pad * 2.0f);
        const view::Spot at = view::Place(where, screen.x, screen.y, wide, tall, std::round(24.0f * scale));
        const ImVec2 a(at.x, at.y);
        const ImVec2 b(at.x + wide, at.y + tall);
        const paint::Ink &ink = paint::ink();
        paint::Box(sheet, a, b, ink.glass);
        paint::Corners(sheet, a, b, fresh ? ink.amber : ink.dim, std::round(8.0f * scale), std::max(1.0f, std::round(scale)));
        float pen = at.y + pad;
        for (const std::string &line : lines)
        {
            paint::Text(sheet, ImVec2(at.x + pad, pen), size, fresh ? ink.text : ink.dim, line.c_str());
            pen += size + gap;
        }
    }
}
