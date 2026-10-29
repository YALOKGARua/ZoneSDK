#include <imgui.h>
#include <reshade.hpp>

#include <zsdk/overlay.h>
#include <zsdk/shell.h>

#include "../detail/engine_core.h"
#include "../detail/lanes.h"
#include "../detail/ledger_raw.h"
#include "../detail/runtime.h"
#include "zsdk_build.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <mutex>
#include <string>

static_assert(RESHADE_API_VERSION == ZSDK_RESHADE_API && IMGUI_VERSION_NUM == ZSDK_IMGUI_NUM, "ZoneSDK pins ReShade API 20 and ImGui 19250");

namespace zsdk::shell
{
    namespace
    {
        constexpr uint64_t kUnseen = ~0ull;

        HMODULE self = nullptr;
        HANDLE guard = nullptr;
        std::atomic<bool> stopping{ false };
        std::atomic<bool> opened{ false };
        std::atomic<bool> booting{ false };
        std::atomic<bool> ready{ false };
        std::atomic<bool> dropped{ false };
        std::atomic<int> runtimes{ 0 };
        std::atomic<uint32_t> off{ 0 };
        std::atomic<uint32_t> keyed{ 0 };
        std::atomic<uint32_t> fresh{ 0 };
        std::atomic<uint32_t> pending{ 0 };
        std::atomic<uint64_t> seen{ kUnseen };
        std::mutex laneGate;
        std::mutex openGate;
        bool live = false;
        bool overlay = false;
        bool gpuPipeline = false;
        bool gpuBind = false;
        bool gpuForget = false;
        std::string title;
        std::string reported;

        const Hooks &H()
        {
            return zsdk::hooks::Get();
        }

        bool Game()
        {
            return exe::Game(exe::File());
        }

        void Taken(const Identity &id)
        {
            std::wstring line = L"ZoneSDK: ";
            for (const char *walk = id.name; walk && *walk; ++walk) line.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
            line += L" does not load: the tag ";
            for (const char *walk = id.tag; walk && *walk; ++walk) line.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
            line += L" is already taken in this process, by a second copy of this add-on or by another add-on with the same tag; every add-on needs a unique tag\n";
            OutputDebugStringW(line.c_str());
        }

        void Unbuilt(const char *why)
        {
            std::wstring line = L"ZoneSDK: ";
            for (const char *walk = app::Get().identity.name; walk && *walk; ++walk) line.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
            line += L" stays off: ";
            for (const char *walk = why; walk && *walk; ++walk) line.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
            line += L"\n";
            OutputDebugStringW(line.c_str());
        }

        void LaneFault(detail::lanes::Lane lane, const detail::Fended &fended)
        {
            namespace lanes = detail::lanes;
            const uint32_t bits = lanes::Spread(lane);
            {
                std::lock_guard<std::mutex> hold(laneGate);
                off.fetch_or(bits);
                fresh.fetch_or(lanes::Bit(lane));
                pending.fetch_or(lanes::Bit(lane));
            }
            const char *scope = bits == lanes::Bit(lane) ? "this lane is off" : bits == lanes::kShared ? "the frame and toasts lanes are off" : "the add-on is off";
            Rare(lanes::kKey[lane], "structured exception 0x%08X at %s in the %s lane, %s for this session and %s is disabled until Try again", fended.code, detail::Where(fended.at, fended.from).c_str(), lanes::kTag[lane], scope,
                 lanes::kKey[lane]);
            ledger::raw::Disable(lanes::kKey[lane]);
            {
                std::lock_guard<std::mutex> hold(laneGate);
                off.fetch_or(bits);
                keyed.fetch_or(lanes::Bit(lane));
                pending.fetch_and(~lanes::Bit(lane));
                seen = kUnseen;
            }
            Flush();
        }

        template<class F> void LaneFence(detail::lanes::Lane lane, F &&work)
        {
            namespace lanes = detail::lanes;
            if (off.load(std::memory_order_relaxed) & lanes::Bit(lane)) return;
            const detail::Fended fended = detail::Fend(work);
            if (fended.thrown) Rare(lanes::kTag[lane], "%s error: %s", lanes::kTag[lane], fended.what.c_str());
            if (fended.code) LaneFault(lane, fended);
        }

        void LaneRestore()
        {
            namespace lanes = detail::lanes;
            const uint32_t early = fresh.load();
            for (uint32_t i = 0; i < lanes::Count; ++i)
                if (early & (1u << i)) ledger::raw::Disable(lanes::kKey[i]);
            uint32_t blocked = 0;
            for (uint32_t i = 0; i < lanes::Count; ++i)
                if (ledger::Blocked(lanes::kKey[i])) blocked |= 1u << i;
            {
                std::lock_guard<std::mutex> hold(laneGate);
                keyed.fetch_or(blocked);
                off.fetch_or(lanes::Restore(blocked));
                seen = kUnseen;
            }
            if (blocked & ~early) Log("safety: the %s lanes are disabled after a crash in an earlier session, Try again in the tab clears them", lanes::Names(blocked & ~early).c_str());
            if (early) Log("safety: the %s lanes faulted before the data folder opened, they stay disabled", lanes::Names(early).c_str());
        }

        void LaneResync()
        {
            namespace lanes = detail::lanes;
            const uint64_t revision = ini::Revision();
            if (revision == seen.load()) return;
            uint32_t back = 0;
            {
                std::lock_guard<std::mutex> hold(laneGate);
                if (revision == seen.load()) return;
                const uint32_t had = keyed.load();
                uint32_t still = 0;
                for (uint32_t i = 0; i < lanes::Count; ++i)
                    if ((had & (1u << i)) && ledger::Blocked(lanes::kKey[i])) still |= 1u << i;
                seen = revision;
                if (still == had) return;
                back = had & ~still;
                keyed = still;
                off = lanes::Off(still | pending.load(), fresh.load());
            }
            Log("safety: the %s lanes run again after Try again%s", lanes::Names(back).c_str(), (fresh.load() & lanes::kStart) ? ", the add-on itself starts again with the next game start" : "");
        }

        std::string Spoken(uint32_t bits, lang::Lang ui)
        {
            namespace lanes = detail::lanes;
            std::string out;
            for (uint32_t i = 0; i < lanes::Count; ++i)
            {
                if (!(bits & (1u << i))) continue;
                if (!out.empty()) out += ", ";
                out += lang::Text(lanes::kRow[i], ui);
            }
            return out;
        }

        void LaneFallback(Overlay &o)
        {
            namespace lanes = detail::lanes;
            const uint32_t named = keyed.load();
            const uint32_t parked = off.load();
            const uint32_t shown = named ? named : (parked & lanes::kStart) ? lanes::Bit(lanes::Boot) : parked;
            const App &a = app::Get();
            ImGui::TextWrapped("%s", lang::Format(lang::Text("sdk.lanes", o.ui), { a.identity.name ? a.identity.name : "", Spoken(shown, o.ui) }).c_str());
            const std::filesystem::path log = ini::LogPath();
            if (!log.empty()) ImGui::TextWrapped("%s", lang::Format(lang::Text("diag.log", o.ui), { log::Shown(log) }).c_str());
            if (named && ImGui::Button((std::string(lang::Text("diag.retry", o.ui)) + "##zsdklanes").c_str())) ledger::Retry();
            if (named) ImGui::SameLine();
            if (ImGui::Button((std::string(lang::Text("diag.report", o.ui)) + "##zsdklanes").c_str())) reported = tab::Report(o);
            if (!reported.empty()) ImGui::TextWrapped("%s", reported.c_str());
            if (fresh.load() & lanes::kStart) ImGui::TextWrapped("%s", lang::Text("sdk.restart", o.ui));
        }

        void OpenOnce()
        {
            if (opened.load()) return;
            std::lock_guard<std::mutex> hold(openGate);
            if (opened.load()) return;
            struct Done
            {
                ~Done()
                {
                    opened = true;
                }
            } done;
            const ini::Schema *schema = app::Get().schema;
            if (!schema || !schema->visit || !schema->make)
            {
                Unbuilt("the add-on has no settings schema (App::schema), so ZoneSDK has no data folder, settings or log for it");
                std::lock_guard<std::mutex> parked(laneGate);
                off.fetch_or(detail::lanes::kPark);
                return;
            }
            if (!ini::Open()) Log("data folder unavailable, settings will not persist");
            lang::Set(ini::Language());
            LaneRestore();
        }

        bool Opening()
        {
            if (!opened.load()) LaneFence(detail::lanes::Boot, [] { OpenOnce(); });
            return opened.load();
        }

        void Boot()
        {
            OpenOnce();
            if (off.load() & detail::lanes::Bit(detail::lanes::Boot)) return;
            if (booting.exchange(true)) return;
            const App &a = app::Get();
            if (H().boot)
            {
                const detail::Fended fended = detail::Fend([] { H().boot(); });
                if (fended.code)
                {
                    LaneFault(detail::lanes::Boot, fended);
                    return;
                }
                if (fended.thrown) Rare(detail::lanes::kTag[detail::lanes::Boot], "%s error: %s", detail::lanes::kTag[detail::lanes::Boot], fended.what.c_str());
            }
            ready = true;
            detail::thread::Start();
            Log("%s %s booted, language %s, pid %lu, zsdk %s manifest %s", a.identity.name, a.identity.version, lang::Code(lang::Current()), GetCurrentProcessId(), ZSDK_VERSION, ZSDK_MANIFEST);
            if (Legacy(a.policy)) Log("zsdk %s legacy policy: %s", ZSDK_VERSION, LegacyText(a.policy).c_str());
        }

        void OnPresent(reshade::api::effect_runtime *runtime)
        {
            if (stopping || !runtime) return;
            LaneFence(detail::lanes::Boot, [] { Boot(); });
            if (!opened) return;
            LaneResync();
            if (!ready) return;
            LaneFence(detail::lanes::Core, [] { detail::thread::Present(); });
            LaneFence(detail::lanes::Frame, [runtime] {
                const Input in = detail::input::Frame(runtime);
                if (H().frame) H().frame(in);
            });
            LaneFence(detail::lanes::Data, [] { ini::Tick(); });
        }

        Overlay Make(reshade::api::effect_runtime *runtime, bool language)
        {
            Overlay o;
            o.runtime = runtime;
            o.ui = language ? overlay::Ui() : lang::Current();
            const float tall = ImGui::GetIO().DisplaySize.y;
            o.scale = tall > 0.0f ? std::max(1.0f, tall / 1080.0f) : 1.0f;
            return o;
        }

        void OnOverlay(reshade::api::effect_runtime *runtime)
        {
            if (stopping || !ready || !runtime) return;
            overlay::Scope scope;
            LaneFence(detail::lanes::Block, [runtime] { paint::Serve(runtime->get_device()); });
            Overlay o = Make(runtime, false);
            if (H().overlay) LaneFence(detail::lanes::Overlay, [&o] { H().overlay(o); });
            LaneFence(detail::lanes::Toasts, [&o] { toast::Draw(o, app::Lane(), H().words, H().tone); });
        }

        void OnPage(reshade::api::effect_runtime *runtime)
        {
            if (stopping || !opened || !runtime) return;
            overlay::Scope scope;
            Overlay o = Make(runtime, true);
            if (off.load() & detail::lanes::Bit(detail::lanes::Page))
            {
                LaneFallback(o);
                return;
            }
            if (!ready) return;
            if (H().page) LaneFence(detail::lanes::Page, [&o] { H().page(o); });
        }

        bool OnOpen(reshade::api::effect_runtime *, bool open, reshade::api::input_source)
        {
            detail::thread::Overlay(open);
            if (!open) tab::Release();
            if (H().opened) LaneFence(detail::lanes::Open, [open] { H().opened(open); });
            return false;
        }

        void OnDropDevice(reshade::api::device *device)
        {
            if (!device) return;
            LaneFence(detail::lanes::Device, [device] { paint::Drop(device); });
            if (!stopping && H().deviceDestroy && Opening()) LaneFence(detail::lanes::DeviceDestroy, [device] { H().deviceDestroy(device); });
        }

        void OnPipeline(reshade::api::device *device, reshade::api::pipeline_layout layout, uint32_t count, const reshade::api::pipeline_subobject *subobjects, reshade::api::pipeline pipeline)
        {
            if (stopping || !device || !H().pipelineInit || !Opening()) return;
            LaneFence(detail::lanes::PipelineInit, [&] { H().pipelineInit(device, layout.handle, count, subobjects, pipeline.handle); });
        }

        void OnBind(reshade::api::command_list *list, reshade::api::pipeline_stage stages, reshade::api::pipeline pipeline)
        {
            if (stopping || !list || !H().pipelineBind || !Opening()) return;
            LaneFence(detail::lanes::PipelineBind, [&] { H().pipelineBind(list, static_cast<uint32_t>(stages), pipeline.handle); });
        }

        void OnForget(reshade::api::device *device, reshade::api::pipeline pipeline)
        {
            if (stopping || !device || !H().pipelineDestroy || !Opening()) return;
            LaneFence(detail::lanes::PipelineDestroy, [&] { H().pipelineDestroy(device, pipeline.handle); });
        }

        void OnMakeRuntime(reshade::api::effect_runtime *)
        {
            runtimes.fetch_add(1);
        }

        void OnDropRuntime(reshade::api::effect_runtime *)
        {
            int count = runtimes.load();
            while (count > 0 && !runtimes.compare_exchange_weak(count, count - 1)) {}
            dropped = true;
            if (stopping) return;
            LaneFence(detail::lanes::Runtime, [] { toast::Reset(); });
            LaneFence(detail::lanes::Save, [] {
                if (ready && ini::Dirty()) ini::Save();
                Flush();
            });
        }

        void Unregister(HMODULE module)
        {
            if (overlay) reshade::unregister_overlay(title.c_str(), &OnPage);
            if (gpuForget) reshade::unregister_event<reshade::addon_event::destroy_pipeline>(&OnForget);
            if (gpuBind) reshade::unregister_event<reshade::addon_event::bind_pipeline>(&OnBind);
            if (gpuPipeline) reshade::unregister_event<reshade::addon_event::init_pipeline>(&OnPipeline);
            gpuForget = gpuBind = gpuPipeline = false;
            reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(&OnDropRuntime);
            reshade::unregister_event<reshade::addon_event::init_effect_runtime>(&OnMakeRuntime);
            reshade::unregister_event<reshade::addon_event::destroy_device>(&OnDropDevice);
            reshade::unregister_event<reshade::addon_event::reshade_open_overlay>(&OnOpen);
            reshade::unregister_event<reshade::addon_event::reshade_overlay>(&OnOverlay);
            reshade::unregister_event<reshade::addon_event::reshade_present>(&OnPresent);
            reshade::unregister_addon(module);
        }
    }

    void *Module()
    {
        return self;
    }

    bool Stopping()
    {
        return stopping;
    }

    bool Ready()
    {
        return ready;
    }

    bool Exiting()
    {
        return detail::Exiting();
    }

    BOOL Main(HMODULE module, DWORD reason, LPVOID reserved, const Hooks &hooks)
    {
        if (reason == DLL_PROCESS_ATTACH)
        {
            if (!Game()) return FALSE;
            const Identity &id = app::Get().identity;
            wchar_t name[96]{};
            swprintf_s(name, MutexShape(id).c_str(), GetCurrentProcessId());
            guard = CreateMutexW(nullptr, FALSE, name);
            if (!guard || GetLastError() == ERROR_ALREADY_EXISTS)
            {
                if (guard) Taken(id);
                if (guard) CloseHandle(guard);
                guard = nullptr;
                return FALSE;
            }
            if (!reshade::register_addon(module))
            {
                CloseHandle(guard);
                guard = nullptr;
                return FALSE;
            }
            self = module;
            title = id.name;
            zsdk::hooks::Bind(hooks);
            DisableThreadLibraryCalls(module);
            reshade::register_event<reshade::addon_event::reshade_present>(&OnPresent);
            reshade::register_event<reshade::addon_event::reshade_overlay>(&OnOverlay);
            reshade::register_event<reshade::addon_event::reshade_open_overlay>(&OnOpen);
            reshade::register_event<reshade::addon_event::destroy_device>(&OnDropDevice);
            reshade::register_event<reshade::addon_event::init_effect_runtime>(&OnMakeRuntime);
            reshade::register_event<reshade::addon_event::destroy_effect_runtime>(&OnDropRuntime);
            if (hooks.pipelineInit)
            {
                reshade::register_event<reshade::addon_event::init_pipeline>(&OnPipeline);
                gpuPipeline = true;
            }
            if (hooks.pipelineBind)
            {
                reshade::register_event<reshade::addon_event::bind_pipeline>(&OnBind);
                gpuBind = true;
            }
            if (hooks.pipelineDestroy)
            {
                reshade::register_event<reshade::addon_event::destroy_pipeline>(&OnForget);
                gpuForget = true;
            }
            reshade::register_overlay(title.c_str(), &OnPage);
            overlay = true;
            live = true;
        }
        else if (reason == DLL_PROCESS_DETACH && live)
        {
            if (reserved) detail::Exit(true);
            if (reserved && ledger::Ended(zsdk::thread::OnGame(), runtimes.load(), dropped.load())) ledger::raw::Leave();
            else if (reserved) ledger::raw::Freeze();
            detail::engine::Dusk();
            stopping = true;
            live = false;
            detail::thread::Stop();
            if (hooks.stop) (void)detail::Fend([&hooks] { hooks.stop(); });
            if (!reserved)
            {
                Unregister(module);
                ledger::Calm();
                ini::Close();
                if (guard) CloseHandle(guard);
                guard = nullptr;
            }
        }
        return TRUE;
    }
}
