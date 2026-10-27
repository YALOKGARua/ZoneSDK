#include <zsdk/overlay.h>

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace zsdk::toast
{
    namespace
    {
        void Strip(ImDrawList *sheet, const ImVec2 &view, float scale, const std::string &text, ImU32 tone, float top, float alpha)
        {
            const float size = std::round(20.0f * scale);
            const float pad = std::round(10.0f * scale);
            const float height = size + pad * 1.2f;
            const float width = std::min(paint::Span(size, text.c_str()) + pad * 2.0f, view.x - 40.0f);
            const float x = std::floor((view.x - width) * 0.5f);
            sheet->AddRectFilled(ImVec2(x, top), ImVec2(x + width, top + height), tab::Fade(tab::kShade, alpha), 4.0f * scale);
            sheet->AddRectFilled(ImVec2(x, top), ImVec2(x + 3.0f * scale, top + height), tab::Fade(tone, alpha));
            sheet->PushClipRect(ImVec2(x, top), ImVec2(x + width, top + height), true);
            paint::Text(sheet, ImVec2(x + pad, top + pad * 0.6f - 1.0f), size, tab::Fade(tab::kText, alpha), text.c_str());
            sheet->PopClipRect();
        }
    }

    void Draw(Overlay &o, float lane, std::string (*words)(const Item &, lang::Lang), uint32_t (*tone)(const Item &))
    {
        if (!o.runtime || !overlay::Inside()) return;
        const uint64_t now = GetTickCount64();
        const std::vector<Item> live = Live(now);
        if (live.empty() || !Focused()) return;
        if (lane <= 0.0f || lane >= 1.0f)
        {
            Rare("toast lane", "toasts: the registry row has lane 0, so %zu toasts are not drawn; set a lane in App::row to show them", live.size());
            return;
        }
        const lang::Lang ui = overlay::Ui();
        ImDrawList *sheet = ImGui::GetForegroundDrawList(nullptr);
        const ImVec2 view = ImGui::GetIO().DisplaySize;
        const float scale = std::clamp(view.y / 1080.0f, 1.0f, 2.0f);
        float y = std::floor(view.y * lane);
        for (const Item &t : live)
        {
            const bool own = t.kind < 0;
            if (!own && !words) continue;
            const uint64_t age = Age(now, t.born);
            const float k = age + kFadeMs > kLifeMs ? static_cast<float>(kLifeMs - std::min(age, kLifeMs)) / static_cast<float>(kFadeMs) : 1.0f;
            const std::string said = own ? std::string(lang::Text("sdk.unsupported", ui)) : words(t, ui);
            Strip(sheet, view, scale * 1.1f, said, own ? tab::kWarn : tone ? tone(t) : tab::kAccent, y, k);
            y += std::round(22.0f * scale) + std::round(10.0f * scale) * 1.2f + 6.0f * scale;
        }
    }
}
