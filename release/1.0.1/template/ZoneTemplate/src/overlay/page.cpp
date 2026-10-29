#include <zsdk/overlay.h>

#include "page.h"

namespace ztemplate::page
{
    namespace
    {
        namespace tab = zsdk::tab;
        zsdk::lang::Lang tongue = zsdk::lang::Lang::Uk;

        const char *T(std::string_view id)
        {
            return zsdk::lang::Text(id, tongue);
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

        std::string Said(const std::string &piece)
        {
            return piece.find('.') != std::string::npos && zsdk::lang::Has(piece) ? std::string(T(piece)) : piece;
        }
    }

    std::string Words(const zsdk::toast::Item &item, zsdk::lang::Lang ui)
    {
        tongue = ui;
        const char *id = item.kind == static_cast<int>(Say::Located) ? "say.located" : item.kind == static_cast<int>(Say::Crashed) ? "say.crashed" : "say.refused";
        return zsdk::lang::Format(T(id), { item.args.empty() ? std::string() : Said(item.args.front()) });
    }

    uint32_t Tone(const zsdk::toast::Item &item)
    {
        return item.kind == static_cast<int>(Say::Located) ? tab::kGood : tab::kWarn;
    }

    void Page(zsdk::Overlay &o)
    {
        tongue = o.ui;
        Settings s = zsdk::ini::Config<Settings>();
        const Settings before = s;
        const Status st = core::Snapshot();
        tab::Label(zsdk::lang::Format(T("title"), { kVersion }), tab::kAccent);
        std::string state = T(PhaseId(st.phase));
        if (st.phase == zsdk::Phase::Waiting && zsdk::lang::Has(st.wait)) state += std::string(". ") + T(st.wait);
        tab::Wrapped(state, st.phase == zsdk::Phase::Ready ? tab::kGood : st.phase == zsdk::Phase::Failed || st.phase == zsdk::Phase::Halted || st.phase == zsdk::Phase::Lost ? tab::kWarn : tab::kDim);
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
        tab::Box("toasts", T("mod.toasts"), s.toasts);
        const zsdk::tab::KeyTexts texts{ .label = T("key.locate") };
        tab::KeyField(o, "locate", s.locate, {}, texts, std::min(tab::Field(), 200.0f));
        ImGui::BeginDisabled(st.phase != zsdk::Phase::Ready || !st.coreReady);
        if (tab::Button("locatebtn", T("btn.locate"))) zsdk::thread::Ask(static_cast<int>(Order::Locate));
        ImGui::EndDisabled();
        if (!st.located.empty()) tab::Wrapped(zsdk::lang::Format(T("say.located"), { st.located }), tab::kGood);
        if (!st.refusal.empty()) tab::Wrapped(zsdk::lang::Format(T("say.refused"), { Said(st.refusal) }), tab::kWarn);
        tab::Diagnostics(o);
        if (!(s == before))
        {
            zsdk::ini::Apply(s);
            zsdk::lang::Set(s.language);
        }
    }
}
