#include <zsdk/overlay.h>

#include "page.h"

#include <algorithm>

namespace zhellohud::page
{
    namespace
    {
        namespace tab = zsdk::tab;
        zsdk::lang::Lang tongue = zsdk::lang::Lang::Uk;

        const char *T(std::string_view id)
        {
            return zsdk::lang::Text(id, tongue);
        }

        std::string Said(const std::string &piece)
        {
            return piece.find('.') != std::string::npos && zsdk::lang::Has(piece) ? std::string(T(piece)) : piece;
        }

        template<class E, size_t N> void Radios(const char *label, const char *prefix, E &value, const char *const (&names)[N])
        {
            tab::Label(T(label), tab::kDim, true);
            ImGui::PushID(prefix);
            for (size_t i = 0; i < N; ++i)
            {
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::RadioButton("##pick", value == static_cast<E>(i))) value = static_cast<E>(i);
                tab::Beside(T(std::string(prefix) + names[i]));
                ImGui::PopID();
            }
            ImGui::PopID();
        }

        ImU32 Shade(zsdk::Phase phase)
        {
            if (phase == zsdk::Phase::Ready) return tab::kGood;
            if (phase == zsdk::Phase::Failed || phase == zsdk::Phase::Halted || phase == zsdk::Phase::Lost) return tab::kWarn;
            return tab::kDim;
        }
    }

    std::string Words(const zsdk::toast::Item &item, zsdk::lang::Lang ui)
    {
        tongue = ui;
        const std::string arg = item.args.empty() ? std::string() : item.args.front();
        if (item.kind == static_cast<int>(Say::Panel)) return T(arg == "on" ? "say.shown" : "say.hidden");
        return zsdk::lang::Format(T("say.crashed"), { Said(arg) });
    }

    uint32_t Tone(const zsdk::toast::Item &item)
    {
        return item.kind == static_cast<int>(Say::Panel) ? tab::kGood : tab::kWarn;
    }

    void Page(zsdk::Overlay &o)
    {
        tongue = o.ui;
        Settings s = zsdk::ini::Config<Settings>();
        const Settings before = s;
        const Status st = core::Snapshot();
        const uint64_t now = zsdk::Now();
        tab::Label(zsdk::lang::Format(T("title"), { kVersion }), tab::kAccent);
        tab::Wrapped(view::State(st, tongue), Shade(st.phase));
        ImGui::Spacing();
        tab::Label(T("lang.label"), tab::kDim, true);
        for (const char *code : { "auto", "uk", "en" })
        {
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::PushID(code);
            if (ImGui::RadioButton("##lang", s.language == code)) s.language = code;
            tab::Beside(T(std::string("lang.") + code));
            ImGui::PopID();
        }
        tab::Box("enabled", T("mod.enabled"), s.enabled);
        tab::Box("shown", T("hud.shown"), s.shown);
        tab::Box("toasts", T("mod.toasts"), s.toasts);
        Radios("hud.corner", "corner.", s.corner, kCorners);
        Radios("hud.unit", "unit.", s.unit, kUnits);
        const zsdk::tab::KeyTexts texts{ .label = T("key.toggle") };
        tab::KeyField(o, "toggle", s.toggle, {}, texts, std::min(tab::Field(), 200.0f));
        ImGui::Spacing();
        const bool fresh = view::Fresh(st, now);
        for (const std::string &line : view::Lines(st, s.unit, now, tongue)) tab::Wrapped(line, fresh ? tab::kText : tab::kDim);
        tab::Diagnostics(o);
        if (!(s == before))
        {
            zsdk::ini::Apply(s);
            zsdk::lang::Set(s.language);
        }
    }
}
