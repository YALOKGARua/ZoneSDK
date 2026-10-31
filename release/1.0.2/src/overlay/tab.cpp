#include <zsdk/overlay.h>

#include "../detail/keys.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace zsdk
{
    namespace
    {
        constexpr uint64_t kReportShownMs = 8000;

        thread_local int inside = 0;
        std::string capturing;
        std::string keyNote;
        std::string reported;
        bool reportSaved = false;
        uint64_t reportedAt = 0;

        bool Off()
        {
            if (inside > 0) return false;
            Rare("paint off-overlay", "paint: a draw call outside the overlay callback was skipped");
            return true;
        }

        const char *Use(const char *given, const char *id, lang::Lang ui)
        {
            return given ? given : lang::Text(id, ui);
        }
    }

    namespace overlay
    {
        bool Inside()
        {
            return inside > 0;
        }

        lang::Lang Ui()
        {
            return paint::Cyrillic() ? lang::Current() : lang::Lang::En;
        }

        Scope::Scope()
        {
            ++inside;
        }

        Scope::~Scope()
        {
            --inside;
        }
    }

    namespace tab
    {
        float Size()
        {
            if (Off()) return 13.0f;
            return ImGui::GetFontSize();
        }

        float Field()
        {
            if (Off()) return 120.0f;
            return std::clamp(ImGui::GetContentRegionAvail().x * 0.45f, 120.0f, 360.0f);
        }

        std::string Fixed(double value, int digits)
        {
            char buffer[48]{};
            _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%.*f", digits, std::isfinite(value) ? value : 0.0);
            return buffer;
        }

        ImU32 Fade(ImU32 tone, float k)
        {
            const float alpha = static_cast<float>((tone >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(k, 0.0f, 1.0f);
            return (tone & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
        }

        void Label(const std::string &text, ImU32 tone, bool framed)
        {
            if (Off()) return;
            const float size = Size();
            const float pad = framed ? ImGui::GetStyle().FramePadding.y : 0.0f;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float width = paint::Span(size, text.c_str());
            paint::Text(ImGui::GetWindowDrawList(), ImVec2(at.x, at.y + pad), size, tone, text.c_str());
            ImGui::Dummy(ImVec2(std::max(width, 1.0f), size + pad * 2.0f));
        }

        void Wrapped(const std::string &text, ImU32 tone)
        {
            if (Off()) return;
            const float room = std::max(ImGui::GetContentRegionAvail().x, 80.0f);
            const float size = Size();
            std::string line;
            size_t from = 0;
            while (from <= text.size())
            {
                size_t space = text.find(' ', from);
                if (space == std::string::npos) space = text.size();
                const std::string word = text.substr(from, space - from);
                const std::string tried = line.empty() ? word : line + " " + word;
                if (!line.empty() && paint::Span(size, tried.c_str()) > room)
                {
                    Label(line, tone);
                    line = word;
                }
                else line = tried;
                from = space + 1;
            }
            if (!line.empty()) Label(line, tone);
        }

        void Beside(const std::string &text, ImU32 tone)
        {
            if (Off()) return;
            ImGui::SameLine(0.0f, -1.0f);
            Label(text, tone, true);
        }

        bool Box(const char *id, const std::string &text, bool &value, ImU32 tone)
        {
            if (Off()) return false;
            ImGui::PushID(id);
            const bool changed = ImGui::Checkbox("##box", &value);
            Beside(text, tone);
            ImGui::PopID();
            return changed;
        }

        bool Whole(const char *id, const char *text, int &value, int low, int high)
        {
            if (Off()) return false;
            ImGui::PushID(id);
            ImGui::SetNextItemWidth(Field());
            const bool changed = ImGui::SliderInt("##int", &value, low, high, "%d", ImGuiSliderFlags_AlwaysClamp);
            Beside(text);
            ImGui::PopID();
            return changed;
        }

        bool Real(const char *id, const char *text, double &value, double low, double high, const char *format, double step)
        {
            if (Off()) return false;
            float shown = static_cast<float>(value);
            ImGui::PushID(id);
            ImGui::SetNextItemWidth(Field());
            const bool changed = ImGui::SliderFloat("##real", &shown, static_cast<float>(low), static_cast<float>(high), format, ImGuiSliderFlags_AlwaysClamp);
            Beside(text);
            ImGui::PopID();
            if (changed) value = std::clamp(std::round(static_cast<double>(shown) / step) * step, low, high);
            return changed;
        }

        bool Button(const char *id, const std::string &text, float width)
        {
            if (Off()) return false;
            const float size = Size();
            const ImGuiStyle &style = ImGui::GetStyle();
            const float span = paint::Span(size, text.c_str());
            const ImVec2 want(std::max(width, span + style.FramePadding.x * 2.0f), size + style.FramePadding.y * 2.0f);
            ImGui::PushID(id);
            const bool hit = ImGui::Button("##button", want);
            const ImVec2 lo = ImGui::GetItemRectMin();
            const ImVec2 hi = ImGui::GetItemRectMax();
            paint::Text(ImGui::GetWindowDrawList(), ImVec2(std::floor((lo.x + hi.x - span) * 0.5f), std::floor((lo.y + hi.y - size) * 0.5f)), size, ImGui::GetColorU32(kText),
                        text.c_str());
            ImGui::PopID();
            return hit;
        }

        bool Clipped(const char *id, const std::string &text, float width)
        {
            if (Off()) return false;
            const float size = Size();
            const ImGuiStyle &style = ImGui::GetStyle();
            const ImVec2 want(std::max(width, 40.0f), size + style.FramePadding.y * 2.0f);
            ImGui::PushID(id);
            const bool hit = ImGui::Button("##clipped", want);
            const ImVec2 lo = ImGui::GetItemRectMin();
            const ImVec2 hi = ImGui::GetItemRectMax();
            ImDrawList *sheet = ImGui::GetWindowDrawList();
            sheet->PushClipRect(lo, hi, true);
            paint::Text(sheet, ImVec2(lo.x + style.FramePadding.x, std::floor((lo.y + hi.y - size) * 0.5f)), size, ImGui::GetColorU32(kText), text.c_str());
            sheet->PopClipRect();
            ImGui::PopID();
            return hit;
        }

        void Heading(const std::string &text)
        {
            if (Off()) return;
            ImGui::Spacing();
            ImGui::Separator();
            Label(text, kAccent);
        }

        bool Capturing(const char *id)
        {
            return id && capturing == id;
        }

        void Release()
        {
            capturing.clear();
            keyNote.clear();
        }

        int KeyField(Overlay &o, const char *id, keys::Chord &chord, std::span<const keys::Chord> others, const KeyTexts &given, float width)
        {
            if (Off() || !o.runtime || !id) return 0;
            const KeyTexts texts{ given.label, Use(given.press, "key.press", o.ui), Use(given.bad, "key.bad", o.ui), Use(given.game, "key.game", o.ui) };
            const bool mine = capturing == id;
            const std::string shown = mine ? std::string(texts.press) : keys::Shown(chord, o.ui);
            int result = 0;
            if (Button(id, shown, width))
            {
                if (mine) capturing.clear();
                else capturing = id;
                keyNote.clear();
            }
            if (texts.label && texts.label[0]) Beside(texts.label);
            if (mine && !ImGui::GetIO().WantTextInput)
            {
                const uint8_t mods = keys::Mods(o.runtime->is_key_down(VK_CONTROL), o.runtime->is_key_down(VK_SHIFT), o.runtime->is_key_down(VK_MENU));
                for (uint32_t key = 0x08; key <= 0xFE; ++key)
                {
                    if (keys::Modifier(static_cast<uint16_t>(key)) || !o.runtime->is_key_pressed(key)) continue;
                    if (key == VK_ESCAPE)
                    {
                        capturing.clear();
                        keyNote.clear();
                        result = -1;
                        break;
                    }
                    keys::Chord picked;
                    picked.key = static_cast<uint16_t>(key);
                    picked.mods = mods;
                    bool taken = false;
                    for (const keys::Chord &other : others) taken = taken || other == picked;
                    if (!keys::Bindable(picked) || taken)
                    {
                        keyNote = texts.bad;
                        break;
                    }
                    chord = picked;
                    capturing.clear();
                    keyNote = detail::keys::Clash(picked) ? texts.game : "";
                    result = 1;
                    break;
                }
            }
            const bool still = capturing == id;
            if (still && !keyNote.empty()) Label(keyNote, keyNote == texts.bad ? kWarn : kAccent);
            else if (!still && detail::keys::Clash(chord)) Label(texts.game, kAccent);
            return result;
        }

        std::string Report(Overlay &o, const DiagTexts &t)
        {
            const std::string text = report::Text(reshade::internal::get_reshade_module_handle());
            if (!Off()) ImGui::SetClipboardText(text.c_str());
            reportSaved = report::Save(text);
            reportedAt = Now();
            reported = reportSaved ? lang::Format(Use(t.copied, "diag.copied", o.ui), { log::Shown(report::File()) }) : std::string(Use(t.unsaved, "diag.unsaved", o.ui));
            return reported;
        }

        void Diagnostics(Overlay &o, const DiagTexts &given)
        {
            if (Off()) return;
            const lang::Lang ui = o.ui;
            const auto pick = [ui](const char *text, const char *id) { return Use(text, id, ui); };
            const DiagTexts t{ pick(given.title, "diag.title"), pick(given.phase, "diag.phase"), pick(given.exe, "diag.exe"), pick(given.known, "diag.known"), pick(given.other, "diag.other"),
                               pick(given.unknown, "diag.unknown"), pick(given.verbs, "diag.verbs"), pick(given.crashed, "diag.crashed"), pick(given.none, "diag.none"), pick(given.retry, "diag.retry"),
                               pick(given.log, "diag.log"), pick(given.sdk, "diag.sdk"), pick(given.yes, "yes"), pick(given.no, "no"), pick(given.markers, "diag.markers"), pick(given.full, "diag.full"),
                               pick(given.pump, "diag.pump"), pick(given.report, "diag.report"), given.copied, given.unsaved, pick(given.unsupported, "sdk.unsupported"), pick(given.foreign, "diag.foreign") };
            ImGui::Spacing();
            if (thread::Rejected()) Wrapped(t.unsupported, kWarn);
            const bool open = ImGui::TreeNodeEx("##zsdkdiag", 0);
            Beside(t.title, kAccent);
            if (!open) return;
            const auto yes = [&t](bool value) { return std::string(value ? t.yes : t.no); };
            const world::Grip w = world::Snapshot();
            Wrapped(lang::Format(t.phase, { yes(thread::Hooked()), yes(thread::Bound()), yes(thread::Wired()), yes(w.stable), std::to_string(w.epoch) }), kDim);
            const thread::Counters c = thread::Count();
            const std::string ago = c.pumpAt ? std::to_string(Age(Now(), c.pumpAt)) : std::string("-");
            Wrapped(lang::Format(t.pump, { std::to_string(c.posted), std::to_string(c.seen), std::to_string(c.ticked), ago, std::to_string(c.wires), Fixed(c.wireMs, 0) }),
                    thread::PumpAlive() ? kDim : kWarn);
            const std::string sha = exe::Sha();
            const std::string build = !exe::Steam()  ? lang::Format(t.foreign, { exe::Name() })
                                      : sha.empty()  ? std::string(t.unknown)
                                      : exe::Known() ? lang::Format(t.known, { exe::Short() })
                                                     : sha.substr(0, 12) + " (" + t.other + ")";
            Wrapped(lang::Format(t.exe, { build }), exe::Steam() ? kDim : kWarn);
            const verbs::Report r = verbs::Last();
            Wrapped(lang::Format(t.verbs, { std::to_string(r.requiredOk), std::to_string(r.requiredAll), std::to_string(r.optionalOk), std::to_string(r.optionalAll) }),
                    r.coreReady ? kDim : kWarn);
            std::string crashed;
            for (const std::string &key : ledger::Disabled()) crashed += (crashed.empty() ? "" : ", ") + key;
            Wrapped(lang::Format(t.crashed, { crashed.empty() ? std::string(t.none) : crashed }), crashed.empty() ? kDim : kWarn);
            if (ledger::Full()) Wrapped(t.full, kWarn);
            if (ini::Folder().empty()) Wrapped(t.markers, kWarn);
            if (!crashed.empty() && Button("zsdkretry", t.retry)) ledger::Retry();
            Wrapped(lang::Format(t.log, { log::Shown(ini::LogPath()) }), kDim);
            const Policy &p = app::Get().policy;
            Wrapped(lang::Format(t.sdk, { ZSDK_VERSION, Legacy(p) ? LegacyText(p) : std::string(t.none) }), kDim);
            if (Button("zsdkreport", t.report)) Report(o, given);
            if (!reported.empty() && Age(Now(), reportedAt) < kReportShownMs) Wrapped(reported, reportSaved ? kGood : kWarn);
            ImGui::TreePop();
        }
    }
}
