#include <zsdk/overlay.h>

#include "page.h"

#include <cstdio>
#include <string>
#include <vector>

namespace zfieldwatch::page
{
    namespace
    {
        namespace tab = zsdk::tab;
        namespace guard = zsdk::guard;
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

        const char *VerdictId(Verdict verdict)
        {
            switch (verdict)
            {
            case Verdict::Pending: return "verdict.pending";
            case Verdict::Waiting: return "verdict.waiting";
            case Verdict::Off: return "verdict.off";
            case Verdict::Crashed: return "verdict.crashed";
            case Verdict::Unmarked: return "verdict.unmarked";
            default: return "verdict.unproven";
            }
        }

        ImU32 Tone(Verdict verdict)
        {
            switch (verdict)
            {
            case Verdict::Read: return tab::kGood;
            case Verdict::Off:
            case Verdict::Crashed:
            case Verdict::Unmarked: return tab::kWarn;
            default: return tab::kDim;
            }
        }

        std::string Hex(uint32_t value)
        {
            char text[16]{};
            std::snprintf(text, sizeof(text), "0x%X", value);
            return text;
        }

        void Field(const guard::Field &field, const Watched &w, const std::string &value, const std::vector<std::string> &proofs, const char *key)
        {
            const Verdict verdict = w.reading.verdict;
            const std::string state = verdict == Verdict::Read ? value : std::string(T(VerdictId(verdict)));
            tab::Wrapped(zsdk::lang::Format(T("field.line"), { field.name, state }), Tone(verdict));
            for (const std::string &proof : proofs) tab::Wrapped(proof, tab::kDim);
            if (!key) return;
            const char *marker = verdict == Verdict::Crashed ? "marker.crashed" : w.proven ? "marker.proven" : "marker.fresh";
            tab::Wrapped(zsdk::lang::Format(T(marker), { key }), verdict == Verdict::Crashed ? tab::kWarn : tab::kDim);
        }

        const char *SetId(guard::State state)
        {
            return state == guard::State::Ok ? "set.on" : state == guard::State::Off ? "set.off" : "set.pending";
        }
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
        tab::Heading(T("head.fields"));
        if (!st.enabled) tab::Wrapped(T("mod.paused"), tab::kDim);
        const guard::Field &fov = fields::kDefaultFov;
        const std::string fovValue = zsdk::lang::Format(T("value.fov"), { tab::Fixed(st.fov.reading.value, 1) });
        Field(fov, st.fov, fovValue, { zsdk::lang::Format(T("proof.reflected"), { std::string(fov.reflectedClass) + "." + fov.reflectedProperty, Hex(fov.at) }) }, kDefaultKey);
        const std::string lockValue = zsdk::lang::Format(T(st.locked.reading.value > 0.0f ? "value.locked" : "value.free"), { tab::Fixed(st.locked.reading.value, 1) });
        std::vector<std::string> lockProofs;
        for (const guard::Field *lock : fields::kLockedFovs)
            lockProofs.push_back(zsdk::lang::Format(T("proof.guard"), { Hex(lock->set->guards[lock->guard].rva), Hex(lock->at), lock->set->name, lock->set->build->id }));
        Field(*fields::kLockedFovs[0], st.locked, lockValue, lockProofs, kLockedKey);
        const guard::Field &view = fields::kViewFov;
        Field(view, Watched{ .reading = { .verdict = st.board.view } }, std::string(), { zsdk::lang::Format(T("proof.unproven"), { Hex(view.at) }) }, nullptr);
        tab::Heading(T("head.build"));
        const Board &b = st.board;
        if (b.build == Build::Unchecked) tab::Wrapped(T("build.unchecked"), tab::kDim);
        else if (b.build == Build::Rejected) tab::Wrapped(T("build.rejected"), tab::kWarn);
        else tab::Wrapped(zsdk::lang::Format(T(b.build == Build::Known ? "build.known" : "build.other"), { b.id }), b.build == Build::Known ? tab::kGood : tab::kWarn);
        tab::Wrapped(zsdk::lang::Format(T(SetId(b.set)), { b.camera ? b.camera->name : fields::kCameraName, T("need.known") }), b.set == guard::State::Ok ? tab::kGood : b.set == guard::State::Off ? tab::kWarn : tab::kDim);
        tab::Diagnostics(o);
        if (!(s == before))
        {
            zsdk::ini::Apply(s);
            zsdk::lang::Set(s.language);
        }
    }
}
