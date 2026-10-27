#include <zsdk/present.h>

#include "../detail/engine_core.h"
#include "../detail/ledger_raw.h"
#include "../detail/native_bindings.h"
#include "../detail/quit.h"
#include "../detail/runtime.h"

#include <windows.h>


#include <atomic>
#include <deque>
#include <mutex>
#include <string>

namespace zsdk
{
    namespace
    {
        constexpr uint64_t kBindMs = 3000;
        constexpr uint64_t kCensusMs = 250;
        constexpr uint64_t kTickMs = 100;
        constexpr uint64_t kAttachMs = 2000;
        constexpr uint64_t kAskMs = 2000;
        constexpr size_t kAskCap = 8;
        constexpr int kAsksPerStep = 2;
        constexpr uint64_t kPublishMs = 250;
        constexpr int kFastWires = 5;
        constexpr int kWireCap = 10;
        constexpr uint64_t kSlowWireMs = 30000;
        constexpr const char *kWireKey = "engine.wire";
        constexpr ULONG kStackSpare = 64 * 1024;
        constexpr DWORD kLeaveMs = 200;
        constexpr double kQuitWaitMs = 100.0;
        constexpr const char *kQuitWhy = "WM_QUIT";
        constexpr const char *kFlagWhy = "GIsRequestingExit";

        struct Request
        {
            int order = 0;
            uint64_t at = 0;
            uint32_t epoch = 0;
        };

        std::atomic<bool> stopping{ false };
        std::atomic<bool> quiet{ false };
        std::atomic<bool> started{ false };
        std::atomic<bool> bound{ false };
        std::atomic<bool> booked{ false };
        std::atomic<bool> rejected{ false };
        std::atomic<bool> busy{ false };
        std::atomic<bool> wakePending{ false };
        std::atomic<bool> hot{ false };
        std::atomic<bool> overlayOpen{ false };
        std::atomic<bool> menuOpen{ false };
        std::atomic<bool> rehash{ false };
        std::atomic<bool> hooked{ false };
        std::atomic<bool> sehHalt{ false };
        std::atomic<bool> wireBlocked{ false };
        std::atomic<bool> wireSpent{ false };
        std::atomic<int> wireTries{ 0 };
        std::atomic<double> wireMs{ 0 };
        std::atomic<unsigned> sehCode{ 0 };
        std::atomic<bool> settling{ false };
        std::atomic<bool> lostNow{ false };
        std::atomic<bool> quitDue{ false };
        std::atomic<const char *> quitWhy{ kQuitWhy };
        std::atomic<const uint8_t *> exitCell{ nullptr };
        std::atomic<uint64_t> pulse{ 0 };
        std::atomic<uint64_t> posted{ 0 };
        std::atomic<uint64_t> seen{ 0 };
        std::atomic<uint64_t> ticked{ 0 };
        std::atomic<uint64_t> presentAt{ 0 };
        std::atomic<uint64_t> excusedAt{ 0 };
        std::atomic<uint64_t> turns{ 0 };
        std::atomic<uint64_t> wakeAt{ 0 };
        std::atomic<uint32_t> knockMs{ 100 };
        std::atomic<HHOOK> hook{ nullptr };
        std::atomic<DWORD> gameThread{ 0 };
        std::atomic<UINT> wakeMessage{ 0 };
        std::atomic<double> lastMs{ 0 };
        std::atomic<double> worstMs{ 0 };
        HANDLE worker = nullptr;
        HANDLE workerWake = nullptr;
        HANDLE workerLeft = nullptr;
        std::mutex hookGate;
        std::mutex askGate;
        std::deque<Request> asks;
        uint64_t wiredAt = 0;
        bool wireSafe = false;
        uint64_t publishedAt = 0;
        uint64_t worstAt = 0;
        bool primed = false;
        thread_local int pumpDepth = 0;
#if defined(ZSDK_TESTS)
        const uint8_t *feigned = nullptr;
#endif

        const Hooks &H()
        {
            return zsdk::hooks::Get();
        }

        template<class F> void Stage(const char *tag, F &&work)
        {
            try
            {
                work();
            }
            catch (const std::exception &error)
            {
                Rare(tag, "%s error: %s", tag, error.what());
            }
        }

        template<class F> void Turned(const char *tag, F &&work)
        {
            try
            {
                work();
            }
            catch (const std::exception &error)
            {
                Rare(tag, "%s error: %s", tag, error.what());
            }
            catch (...)
            {
                Rare(tag, "%s error: unknown exception", tag);
            }
        }

        template<class F> void Walled(const char *tag, F &&work)
        {
            const detail::Fended fended = detail::Fend(work);
            if (fended.thrown) Rare(tag, "%s error: %s", tag, fended.what.c_str());
            if (fended.code) Rare(tag, "%s: structured exception 0x%08X at the game exit, the exit goes on", tag, fended.code);
        }

        void Quit()
        {
            if (H().quit) Walled("hooks.quit", [] { H().quit(); });
            Walled("ledger calm", [] { ledger::Calm(); });
            Log("game exit requested (%s)", quitWhy.load());
            Flush();
        }

        void Finish()
        {
            if (quitDue.exchange(false)) Quit();
        }

        void Leave(const char *why, bool patient)
        {
            if (quiet.exchange(true) || stopping) return;
            quitWhy = why;
            detail::engine::Dusk();
            quitDue = true;
            if (patient)
            {
                const double from = Ms();
                while (busy.load() && Ms() - from < kQuitWaitMs) Sleep(1);
                if (busy.load()) Log("exit sequence handed to the game thread: a tick still runs after %.0f ms (%s)", kQuitWaitMs, why);
            }
            if (!busy.load()) Finish();
        }

        bool Raised()
        {
            const uint8_t *cell = exitCell.load();
            uint8_t value = 0;
            return cell && ue::Copy(cell, &value, sizeof(value)) && value != 0;
        }

        void Poll(bool patient)
        {
            if (!quiet && !stopping && Raised()) Leave(kFlagWhy, patient);
        }

        void Arm()
        {
            exitCell = nullptr;
#if defined(ZSDK_TESTS)
            if (feigned)
            {
                exitCell = feigned;
                Log("exit watcher on: a test flag cell stands for GIsRequestingExit");
                return;
            }
#endif
            constexpr size_t kSets = std::size(detail::quit::kExits);
            guard::Report r[kSets];
            for (size_t i = 0; i < kSets; ++i) r[i] = guard::Check(*detail::quit::kExits[i].set);
            for (size_t i = 0; i < kSets; ++i)
            {
                uintptr_t at = 0;
                if (r[i].state != guard::State::Ok || !guard::Use(*detail::quit::kExits[i].flag, at)) continue;
                exitCell = reinterpret_cast<const uint8_t *>(exe::Base() + at);
                Log("exit watcher on: GIsRequestingExit at exe+0x%X, guard set %s %d/%d match", static_cast<unsigned>(at), detail::quit::kExits[i].set->name, r[i].ok, r[i].total);
                return;
            }
            size_t shown = 0;
            bool fits = false;
            for (size_t i = 0; i < kSets && !fits; ++i)
            {
                fits = r[i].imageOk;
                if (fits || r[i].ok > r[shown].ok) shown = i;
            }
            Log("exit watcher off: guard set %s %d/%d match, image 0x%X, only WM_QUIT and the exit mark remain", fits ? detail::quit::kExits[shown].set->name : detail::quit::kName, r[shown].ok, r[shown].total,
                r[shown].image);
        }

        detail::thread::Wired GiveUp()
        {
            if (!wireSpent.exchange(true))
            {
                const char *snag = detail::engine::Snag();
                Log("engine wiring gave up after %d attempts (%s), the engine stays off for this session", wireTries.load(), snag && *snag ? snag : "-");
            }
            return detail::thread::Wired::Spent;
        }

        bool WireDue(uint64_t now)
        {
            const Policy &p = app::Get().policy;
            if (!wiredAt) return true;
            if (p.wire == WireRetry::Fixed) return Age(now, wiredAt) >= p.wireMs;
            return Age(now, wiredAt) >= (wireTries < kFastWires ? p.wireMs : kSlowWireMs);
        }

        void Requests(uint64_t now)
        {
            for (int i = 0; i < kAsksPerStep; ++i)
            {
                Request r;
                {
                    std::lock_guard<std::mutex> hold(askGate);
                    if (asks.empty()) return;
                    r = asks.front();
                    asks.pop_front();
                }
                const uint64_t age = Age(now, r.at);
                const uint32_t epoch = world::Hold().epoch;
                if (age > kAskMs || r.epoch != epoch)
                {
                    Log("request %s dropped (%llu ms old, epoch %u/%u)", H().orderName ? H().orderName(r.order) : "order", static_cast<unsigned long long>(age), r.epoch, epoch);
                    continue;
                }
                if (H().order) H().order(r.order);
            }
        }

        bool Rehashed()
        {
            const bool again = rehash.exchange(false);
            if (again) guard::Recheck();
            return again;
        }

        void Prime()
        {
            if (H().refresh) H().refresh(Rehashed());
            verbs::Prime(app::Get().specs);
            for (const guard::Set *set : app::Get().sets)
            {
                if (!set) continue;
                const guard::Report r = guard::Check(*set);
                Log("guard set %s: %d/%d match, image 0x%X, %s%s%s", set->name ? set->name : "-", r.ok, r.total, r.image, r.state == guard::State::Ok ? "on" : r.state == guard::State::Off ? "off" : "pending",
                    r.bad.empty() ? "" : ", failed ", r.bad.c_str());
            }
            if (H().prime) H().prime();
        }

        void Publish(uint64_t now, bool force)
        {
            if (!force && Age(now, publishedAt) < kPublishMs) return;
            publishedAt = now;
            const world::Grip &w = world::Hold();
            settling = w.fit && !w.stable;
            detail::world::Seen();
            if (H().publish) Stage("publish", [now] { H().publish(now); });
        }

        void Step()
        {
            if (quiet || !bound || !booked) return;
            const uint64_t now = GetTickCount64();
            if (!detail::engine::Ready())
            {
                detail::thread::Wiring(now, &detail::engine::Wire);
                return;
            }
            if (!primed)
            {
                primed = true;
                Prime();
            }
            if (zsdk::detail::Halted())
            {
                Publish(now, false);
                return;
            }
            if (!detail::world::Advance()) return;
            if (H().advance) H().advance();
            Stage("watch", [now] { detail::world::Watch(now); });
            if (H().refresh) H().refresh(Rehashed());
            Stage("requests", [now] { Requests(now); });
            if (H().step) Stage("tick", [now] { H().step(now); });
            hot = H().hot ? H().hot() : false;
            Publish(now, hot.load());
        }

        bool Fenced(std::string &where)
        {
            const detail::Fended fended = detail::Fend([] { Step(); });
            if (fended.thrown) Rare("game thread error", "game thread error: %s", fended.what.c_str());
            if (!fended.code) return true;
            sehCode = fended.code;
            where = detail::Where(fended.at);
            return false;
        }

        void Pump()
        {
            if (stopping || quiet || !started || GetCurrentThreadId() != gameThread.load()) return;
            Poll(false);
            if (quiet || busy.exchange(true)) return;
            pulse = GetTickCount64();
            ++ticked;
            const double from = Ms();
            std::string where;
            ++pumpDepth;
            const bool clean = Fenced(where);
            --pumpDepth;
            if (!clean)
            {
                Rare("game thread seh", "structured exception 0x%08X at %s in the game thread tick", sehCode.load(), where.c_str());
                if (app::Get().policy.haltOnSeh && !sehHalt.exchange(true))
                {
                    detail::Halt();
                    Log("engine calls are halted for this session after a structured exception");
                }
            }
            const double ms = Ms() - from;
            const uint64_t now = GetTickCount64();
            lastMs = ms;
            if (ms > worstMs.load() || Age(now, worstAt) > 10000)
            {
                worstMs = ms;
                worstAt = now;
            }
            if (ms > 5.0) Rare("slow tick", "slow tick %.2f ms", ms);
            busy = false;
            Finish();
        }

        LRESULT CALLBACK Message(int code, WPARAM wparam, LPARAM lparam)
        {
            if (code == HC_ACTION && (wparam & PM_REMOVE) != 0 && lparam)
            {
                MSG &msg = *reinterpret_cast<MSG *>(lparam);
                if (msg.message == WM_QUIT) Leave(kQuitWhy, false);
                const UINT wake = wakeMessage.load();
                if (wake != 0 && msg.message == wake && msg.hwnd == nullptr)
                {
                    ++seen;
                    wakePending = false;
                    if (!stopping && !quiet) Pump();
                    msg.message = WM_NULL;
                }
            }
            return CallNextHookEx(hook.load(), code, wparam, lparam);
        }

        struct Hunt
        {
            DWORD thread = 0;
            long area = 0;
        };

        BOOL CALLBACK Inspect(HWND window, LPARAM param)
        {
            Hunt &hunt = *reinterpret_cast<Hunt *>(param);
            DWORD pid = 0;
            const DWORD tid = GetWindowThreadProcessId(window, &pid);
            if (pid != GetCurrentProcessId() || tid == 0 || !IsWindowVisible(window)) return TRUE;
            wchar_t kind[64]{};
            GetClassNameW(window, kind, 64);
            if (wcscmp(kind, L"UnrealWindow") != 0) return TRUE;
            RECT box{};
            GetWindowRect(window, &box);
            const long area = (box.right - box.left) * (box.bottom - box.top);
            if (area > hunt.area)
            {
                hunt.area = area;
                hunt.thread = tid;
            }
            return TRUE;
        }

        bool Attach()
        {
            std::lock_guard<std::mutex> hold(hookGate);
            if (stopping) return false;
            if (hook.load()) return true;
            Hunt hunt;
            EnumWindows(Inspect, reinterpret_cast<LPARAM>(&hunt));
            if (!hunt.thread) return false;
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&Message), &self);
            HHOOK made = SetWindowsHookExW(WH_GETMESSAGE, Message, self, hunt.thread);
            if (!made)
            {
                const DWORD first = GetLastError();
                made = SetWindowsHookExW(WH_GETMESSAGE, Message, nullptr, hunt.thread);
                if (!made)
                {
                    Log("game thread hook failed: %lu / %lu", first, GetLastError());
                    return false;
                }
            }
            gameThread = hunt.thread;
            hook = made;
            hooked = true;
            if (stopping)
            {
                if (const HHOOK gone = hook.exchange(nullptr)) UnhookWindowsHookEx(gone);
                gameThread = 0;
                hooked = false;
                return false;
            }
            Log("hook: game thread %lu, message %u", hunt.thread, wakeMessage.load());
            return true;
        }

        void Unfit(const std::string &why)
        {
            rejected = true;
            Log("this game build is not supported: %s", why.c_str());
            Log("the census thread keeps ticking without engine work");
            toast::Push(toast::kUnsupported, why, {}, true);
        }

        void BindStep()
        {
            if (!exe::Steam())
            {
                Unfit(exe::Name() + " is not the Steam build Stalker2-Win64-Shipping.exe, game calls stay off for this session");
                return;
            }
            try
            {
                if (detail::engine::Bind())
                {
                    bound = true;
                    Log("engine bound");
                    return;
                }
            }
            catch (const std::exception &error)
            {
                Log("bind error: %s", error.what());
            }
            if (detail::native_bindings::State() != detail::native_bindings::Status::Rejected) return;
            Unfit("native bindings rejected");
        }

        void HashStep()
        {
            const double from = Ms();
            const std::string sha = exe::Hash();
            Log("exe: %s %s, %.0f ms", sha.empty() ? "unavailable" : sha.c_str(), guard::Find(sha) ? "known" : "unknown build, the verb check decides", Ms() - from);
            rehash = true;
        }

        void StockStep()
        {
            try
            {
                const double from = Ms();
                detail::engine::Survey();
                detail::engine::Restock();
                booked = true;
                Log("function book ready on the census thread in %.0f ms", Ms() - from);
                thread::Wake();
            }
            catch (const std::exception &error)
            {
                Rare("function book error", "function book error: %s, the census thread tries again in %llu s", error.what(), static_cast<unsigned long long>(kBindMs / 1000));
            }
        }

        void ServeStep()
        {
            try
            {
                const double from = Ms();
                if (!detail::engine::Serve()) return;
                Note(1, "function book renewed for wiring on the census thread in %.0f ms", Ms() - from);
                thread::Wake();
            }
            catch (const std::exception &error)
            {
                Rare("book serve error", "function book error for wiring: %s", error.what());
            }
        }

        void CensusStep(uint64_t &sweptAt, uint32_t &sweptEpoch)
        {
            try
            {
                detail::census::Tally(sweptAt, sweptEpoch);
            }
            catch (const std::exception &error)
            {
                Rare("census error", "census error: %s", error.what());
            }
            try
            {
                if (!stopping && detail::engine::Refresh()) Log("function book refreshed after a missing function, epoch %u", detail::engine::BookEpoch());
            }
            catch (const std::exception &error)
            {
                Rare("book refresh error", "function book refresh error: %s", error.what());
            }
        }

        DWORD WINAPI Work(LPVOID)
        {
            ULONG spare = kStackSpare;
            if (!SetThreadStackGuarantee(&spare)) Log("census thread stack guarantee refused: %lu", GetLastError());
            uint64_t bindAt = 0;
            uint64_t stockAt = 0;
            uint64_t censusAt = 0;
            uint64_t sweptAt = 0;
            uint32_t sweptEpoch = 0xFFFFFFFFu;
            bool hashed = false;
            while (!stopping)
            {
                const uint64_t now = GetTickCount64();
                ++turns;
                Turned("exit poll", [] { Poll(true); });
                if (!quiet && !bound && !rejected && (!bindAt || Age(now, bindAt) >= kBindMs))
                {
                    bindAt = now;
                    Turned("bind step", [] { BindStep(); });
                }
                if (stopping) break;
                if (!quiet && bound && !hashed)
                {
                    hashed = true;
                    Turned("hash step", [] { HashStep(); });
                }
                if (stopping) break;
                if (!quiet && bound && !booked && (!stockAt || Age(now, stockAt) >= kBindMs))
                {
                    stockAt = now;
                    Turned("stock step", [] { StockStep(); });
                }
                if (stopping) break;
                if (!quiet && booked && detail::engine::Wanted()) Turned("serve step", [] { ServeStep(); });
                if (stopping) break;
                if (!quiet && booked && Age(now, censusAt) >= kCensusMs)
                {
                    censusAt = now;
                    Turned("census step", [&sweptAt, &sweptEpoch] { CensusStep(sweptAt, sweptEpoch); });
                }
                if (stopping) break;
                Turned("exit poll", [] { Poll(true); });
                Turned("job step", [] { detail::job::Run(GetTickCount64()); });
                if (stopping) break;
                WaitForSingleObject(workerWake, static_cast<DWORD>(kTickMs));
            }
            if (workerLeft) SetEvent(workerLeft);
            return 0;
        }

        void Name(HANDLE thread)
        {
            using Describe = HRESULT(WINAPI *)(HANDLE, PCWSTR);
            const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
            const FARPROC found = kernel ? GetProcAddress(kernel, "SetThreadDescription") : nullptr;
            if (!found || !thread) return;
            std::wstring name;
            for (const char *walk = app::Get().identity.name; walk && *walk; ++walk) name.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*walk)));
            name += L" census";
            const HRESULT result = reinterpret_cast<Describe>(reinterpret_cast<void *>(found))(thread, name.c_str());
            if (FAILED(result)) Log("census thread name refused: 0x%08lX", static_cast<unsigned long>(result));
        }

        bool LostAt(uint64_t now)
        {
            if (stopping || quiet || !started) return false;
            return detail::thread::Lost(detail::thread::Beat{ hooked.load(), pulse.load(), presentAt.load(), excusedAt.load() }, now);
        }

        uint32_t KnockMs()
        {
            const Policy &p = app::Get().policy;
            if (p.knock == Knock::BusyIdle) return hot.load() ? p.knockMs : p.idleMs;
            if (p.knock == Knock::Fixed) return p.knockMs;
            return knockMs.load();
        }
    }

    namespace detail::thread
    {
        bool InPump()
        {
            return pumpDepth > 0;
        }

        Scope::Scope()
        {
            ++pumpDepth;
        }

        Scope::~Scope()
        {
            --pumpDepth;
        }

        Wired WireStep(bool (*wire)())
        {
            if (ledger::Blocked(kWireKey))
            {
                if (!wireBlocked.exchange(true)) Log("engine wiring is disabled after a crash during wiring (%s), Try again in the tab clears it", kWireKey);
                return Wired::Blocked;
            }
            if (wireBlocked.exchange(false)) Log("engine wiring is allowed again");
            const std::string build = exe::Short();
            const bool known = !build.empty() && build != "?";
            const bool first = !wireSafe && (!known || ledger::Proven(kWireKey) != build) && !ledger::Preloaded(kWireKey);
            if (first && !ledger::raw::Arm(kWireKey) && app::Get().policy.strictMarks)
            {
                ledger::raw::Settle();
                Rare("crash marker", "safety: the crash marker for %s could not be written, wiring waits", kWireKey);
                return Wired::Refused;
            }
            bool done = false;
            try
            {
                done = wire && wire();
            }
            catch (...)
            {
                if (first) ledger::raw::Settle();
                throw;
            }
            wireSafe = true;
            if (first)
            {
                if (done) ledger::raw::Prove(kWireKey, build);
                else ledger::raw::Settle();
            }
            return done ? Wired::Done : Wired::Failed;
        }

        bool WireBlocked()
        {
            return wireBlocked.load();
        }

        Wired Wiring(uint64_t now, bool (*wire)())
        {
            if (wireSpent) return Wired::Spent;
            if (wireTries >= kWireCap) return GiveUp();
            if (!WireDue(now)) return Wired::Idle;
            if (detail::engine::Stale())
            {
                if (!detail::engine::Wanted())
                {
                    detail::engine::Want();
                    if (workerWake) SetEvent(workerWake);
                }
                return Wired::Stale;
            }
            wiredAt = now;
            ++wireTries;
            const double from = Ms();
            const Wired done = WireStep(wire);
            const double ms = Ms() - from;
            wireMs = ms;
            if (done == Wired::Done)
            {
                Log("engine wired on game thread %lu in %.0f ms", GetCurrentThreadId(), ms);
                return done;
            }
            if (done != Wired::Failed)
            {
                --wireTries;
                return done;
            }
            if (wireTries >= kWireCap) return GiveUp();
            return done;
        }

        uint64_t Turns()
        {
            return turns.load();
        }

        bool Watching()
        {
            return exitCell.load() != nullptr;
        }

#if defined(ZSDK_TESTS)
        void Cell(const uint8_t *cell)
        {
            feigned = cell;
        }
#endif

        void Start()
        {
            if (started.exchange(true)) return;
            stopping = false;
            quiet = false;
            Arm();
            const Policy &p = app::Get().policy;
            knockMs = p.knockMs;
            wakeMessage = RegisterWindowMessageW(WakeName(app::Get().identity).c_str());
            HMODULE self = nullptr;
            const bool pinned = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&Work), &self) != 0;
            const DWORD pinError = pinned ? 0 : GetLastError();
            workerWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            workerLeft = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            detail::job::Seal();
            if (pinned)
            {
                worker = CreateThread(nullptr, 0, Work, nullptr, 0, nullptr);
                if (!worker) Log("census thread failed: %lu", GetLastError());
                Name(worker);
            }
            else
            {
                rejected = true;
                Log("module pin failed: %lu, the census thread is not started, so an unload can never unmap code it runs; game calls and jobs stay off for this session", pinError);
            }
            Log("core started, wake message 0x%X", wakeMessage.load());
        }

        void Stop()
        {
            stopping = true;
            quiet = true;
            hooked = false;
            {
                std::unique_lock<std::mutex> hold(hookGate, std::try_to_lock);
                gameThread = 0;
                if (const HHOOK gone = hook.exchange(nullptr)) UnhookWindowsHookEx(gone);
            }
            if (workerWake) SetEvent(workerWake);
            if (detail::Exiting()) return;
            if (worker && workerLeft) WaitForSingleObject(workerLeft, kLeaveMs);
            if (worker)
            {
                CloseHandle(worker);
                worker = nullptr;
            }
        }

        bool Gap(uint64_t before, uint64_t now)
        {
            return before != 0 && Age(now, before) > kDrawnMs;
        }

        bool Lost(const Beat &beat, uint64_t now)
        {
            if (!beat.hooked || !beat.pumpAt || !beat.presentAt) return false;
            if (Age(now, beat.presentAt) > kDrawnMs) return false;
            return Age(now, beat.pumpAt > beat.excusedAt ? beat.pumpAt : beat.excusedAt) >= kLostMs;
        }

        void Present()
        {
            if (stopping || !started) return;
            static uint64_t hunted = 0;
            static uint64_t knocked = 0;
            static uint64_t lostPulse = 0;
            const uint64_t now = GetTickCount64();
            if (!hook.load() && Age(now, hunted) >= kAttachMs)
            {
                hunted = now;
                Attach();
            }
            const uint64_t before = presentAt.exchange(now);
            if (detail::thread::Gap(before, now) || overlayOpen || menuOpen || zsdk::world::Paused() || settling || busy) excusedAt = now;
            detail::job::Stuck(now);
            const bool lost = LostAt(now);
            if (lost != lostNow.exchange(lost))
            {
                const uint64_t last = pulse.load();
                if (lost)
                {
                    lostPulse = last;
                    Log("game thread pump lost: no tick for %.1f s while presents run, posted %llu, seen %llu, ticked %llu", static_cast<double>(Age(now, last)) / 1000.0,
                        static_cast<unsigned long long>(posted.load()), static_cast<unsigned long long>(seen.load()), static_cast<unsigned long long>(ticked.load()));
                }
                else if (last != lostPulse) Log("game thread pump is back after %.1f s without a tick", static_cast<double>(Age(last, lostPulse)) / 1000.0);
                else Log("game thread pump: the gap is excused now (overlay, menu, pause, loading or stop), last tick %.1f s ago", static_cast<double>(Age(now, last)) / 1000.0);
            }
            bool active = hot.load();
            if (!active)
            {
                std::lock_guard<std::mutex> hold(askGate);
                active = !asks.empty();
            }
            if (!quiet && (active || Age(now, knocked) >= KnockMs()))
            {
                knocked = now;
                zsdk::thread::Wake();
            }
        }

        void Overlay(bool open)
        {
            overlayOpen = open;
        }

        void Menu(bool open)
        {
            menuOpen = open;
        }
    }

    namespace thread
    {
        bool OnGame()
        {
            const DWORD id = gameThread.load();
            return id != 0 && GetCurrentThreadId() == id;
        }

        bool Started()
        {
            return started.load();
        }

        bool Hooked()
        {
            return hooked.load();
        }

        bool Bound()
        {
            return bound.load();
        }

        bool Booked()
        {
            return booked.load();
        }

        bool Rejected()
        {
            return rejected.load();
        }

        bool Wired()
        {
            return detail::engine::Ready();
        }

        bool Halted()
        {
            return sehHalt.load() || zsdk::detail::Halted();
        }

        bool Stopping()
        {
            return stopping.load() || quiet.load();
        }

        bool Overlaid()
        {
            return overlayOpen.load();
        }

        void Wake()
        {
            const DWORD target = gameThread.load();
            const UINT wake = wakeMessage.load();
            if (!target || !wake || stopping || quiet) return;
            const uint64_t now = GetTickCount64();
            if (wakePending.exchange(true) && Age(now, wakeAt.load()) < 1000) return;
            wakeAt = now;
            if (!PostThreadMessageW(target, wake, 0, 0)) wakePending = false;
            else ++posted;
        }

        void Ask(int order)
        {
            {
                std::lock_guard<std::mutex> hold(askGate);
                if (asks.size() >= kAskCap) asks.pop_front();
                asks.push_back(Request{ order, GetTickCount64(), world::Epoch() });
            }
            Wake();
        }

        void Knock(uint32_t ms)
        {
            knockMs = ms;
        }

        double LastMs()
        {
            return lastMs.load();
        }

        double WorstMs()
        {
            return worstMs.load();
        }

        uint64_t LastPumpAt()
        {
            return pulse.load();
        }

        bool PumpAlive()
        {
            const uint64_t at = pulse.load();
            return at != 0 && Age(GetTickCount64(), at) < detail::thread::kLostMs;
        }

        Counters Count()
        {
            Counters c;
            c.posted = posted.load();
            c.seen = seen.load();
            c.ticked = ticked.load();
            c.pumpAt = pulse.load();
            c.turns = turns.load();
            c.wires = wireTries.load();
            c.wireMs = wireMs.load();
            return c;
        }

        Phase State(bool stable)
        {
            if (LostAt(GetTickCount64())) return Phase::Lost;
            if (rejected || wireBlocked || wireSpent) return Phase::Failed;
            if (!started) return Phase::Off;
            if (!hooked) return Phase::Hooking;
            if (!bound) return Phase::Binding;
            if (Halted()) return Phase::Halted;
            if (!detail::engine::Ready()) return Phase::Wiring;
            if (!stable) return Phase::Waiting;
            return Phase::Ready;
        }
    }
}
