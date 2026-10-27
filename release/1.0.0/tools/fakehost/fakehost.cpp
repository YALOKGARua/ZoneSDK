#include <imgui.h>
#include <reshade.hpp>

#include <windows.h>

#include <tlhelp32.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fake
{
    struct Keys
    {
        bool Down(uint32_t) const
        {
            return false;
        }

        bool Pressed(uint32_t) const
        {
            return false;
        }
    };

    enum class Phase : int { Load, Present, Overlay, Open, Count };

    std::mutex gate;
    std::vector<std::string> journal;
    std::vector<std::pair<int, void *>> live;
    std::vector<std::string> traps;
    std::string overlayTitle;
    void *overlayCallback = nullptr;
    std::atomic<int> imguiCalls{ 0 };
    std::atomic<int> registers{ 0 };
    std::atomic<int> phase{ 0 };
    std::atomic<int> byPhase[static_cast<int>(Phase::Count)]{};
    std::atomic<int> frames{ 0 };
    DWORD mainThread = 0;
    int byName[512]{};
    const char *names[512]{};

    void Note(const std::string &line)
    {
        std::lock_guard<std::mutex> hold(gate);
        journal.push_back(line);
    }

    const char *EventName(int ev)
    {
        using reshade::addon_event;
        switch (static_cast<addon_event>(ev))
        {
        case addon_event::reshade_present: return "reshade_present";
        case addon_event::reshade_overlay: return "reshade_overlay";
        case addon_event::reshade_open_overlay: return "reshade_open_overlay";
        case addon_event::destroy_device: return "destroy_device";
        case addon_event::init_effect_runtime: return "init_effect_runtime";
        case addon_event::destroy_effect_runtime: return "destroy_effect_runtime";
        case addon_event::init_pipeline: return "init_pipeline";
        case addon_event::bind_pipeline: return "bind_pipeline";
        case addon_event::destroy_pipeline: return "destroy_pipeline";
        default: return "other";
        }
    }

    void Trap(int index, const char *name)
    {
        std::lock_guard<std::mutex> hold(gate);
        traps.push_back(std::to_string(index) + ":" + name);
    }

    void Hit(int index, const char *name)
    {
        ++imguiCalls;
        ++byPhase[phase.load()];
        if (index >= 0 && index < 512)
        {
            ++byName[index];
            names[index] = name;
        }
    }

    template<class T> T &Store()
    {
        alignas(16) static unsigned char bytes[sizeof(T)]{};
        return *reinterpret_cast<T *>(bytes);
    }

    ImGuiIO &Io()
    {
        static bool made = false;
        ImGuiIO &io = Store<ImGuiIO>();
        if (!made)
        {
            made = true;
            io.DisplaySize = ImVec2(1920.0f, 1080.0f);
            io.DeltaTime = 1.0f / 60.0f;
            io.FontGlobalScale = 1.0f;
        }
        return io;
    }

    ImGuiStyle &Style()
    {
        static bool made = false;
        ImGuiStyle &style = Store<ImGuiStyle>();
        if (!made)
        {
            made = true;
            style.Alpha = 1.0f;
            style.FramePadding = ImVec2(4.0f, 3.0f);
            style.ItemSpacing = ImVec2(8.0f, 4.0f);
            style.WindowPadding = ImVec2(8.0f, 8.0f);
        }
        return style;
    }

    ImDrawList *List()
    {
        return &Store<ImDrawList>();
    }

    ImFont *Font()
    {
        return &Store<ImFont>();
    }

    ImGuiViewport *View()
    {
        ImGuiViewport &view = Store<ImGuiViewport>();
        view.Size = ImVec2(1920.0f, 1080.0f);
        view.WorkSize = view.Size;
        return &view;
    }

    double Seconds()
    {
        return static_cast<double>(GetTickCount64()) / 1000.0;
    }

    int Frames()
    {
        return frames.load();
    }

#include "imgui.inc"

    imgui_function_table_19250 table{};
}

namespace reshade::api
{
    struct FakeRuntime final : public effect_runtime
    {
        fake::Keys keys;

        static void Trap(int index, const char *name)
        {
            fake::Trap(index, name);
        }

#include "runtime.inc"
    };
}

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void *, uint32_t api)
{
    ++fake::registers;
    fake::Note("register_addon api " + std::to_string(api));
    return api == RESHADE_API_VERSION;
}

extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(void *)
{
    fake::Note("unregister_addon");
}

extern "C" __declspec(dllexport) void ReShadeRegisterEvent(reshade::addon_event ev, void *callback)
{
    {
        std::lock_guard<std::mutex> hold(fake::gate);
        fake::live.emplace_back(static_cast<int>(ev), callback);
    }
    fake::Note(std::string("register_event ") + fake::EventName(static_cast<int>(ev)));
}

extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback)
{
    {
        std::lock_guard<std::mutex> hold(fake::gate);
        for (auto it = fake::live.begin(); it != fake::live.end(); ++it)
            if (it->first == static_cast<int>(ev) && it->second == callback)
            {
                fake::live.erase(it);
                break;
            }
    }
    fake::Note(std::string("unregister_event ") + fake::EventName(static_cast<int>(ev)));
}

extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *title, void (*callback)(reshade::api::effect_runtime *))
{
    fake::overlayTitle = title ? title : "";
    fake::overlayCallback = reinterpret_cast<void *>(callback);
    fake::Note("register_overlay " + fake::overlayTitle);
}

extern "C" __declspec(dllexport) void ReShadeUnregisterOverlay(const char *title, void (*)(reshade::api::effect_runtime *))
{
    fake::Note(std::string("unregister_overlay ") + (title ? title : ""));
}

extern "C" __declspec(dllexport) const imgui_function_table *ReShadeGetImGuiFunctionTable(uint32_t version)
{
    fake::Note("imgui_table " + std::to_string(version));
    return version == IMGUI_VERSION_NUM ? reinterpret_cast<const imgui_function_table *>(&fake::table) : nullptr;
}

extern "C" __declspec(dllexport) void ReShadeLogMessage(void *, int level, const char *message)
{
    fake::Note("log " + std::to_string(level) + " " + (message ? message : ""));
}

extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void *, reshade::api::effect_runtime *, const char *, const char *, char *, size_t *size)
{
    if (size) *size = 0;
    return false;
}

extern "C" __declspec(dllexport) void ReShadeGetBasePath(char *path, size_t *size)
{
    if (path && size && *size) path[0] = 0;
    if (size) *size = 0;
}

namespace
{
    bool Alone()
    {
        const DWORD self = GetCurrentProcessId();
        const HANDLE shot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (shot == INVALID_HANDLE_VALUE) return false;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        bool alone = true;
        for (BOOL more = Process32FirstW(shot, &entry); more; more = Process32NextW(shot, &entry))
            if (entry.th32ProcessID != self && _wcsicmp(entry.szExeFile, L"Stalker2-Win64-Shipping.exe") == 0) alone = false;
        CloseHandle(shot);
        return alone;
    }

    std::string lastDebug;

    LONG CALLBACK Debugged(EXCEPTION_POINTERS *info)
    {
        const EXCEPTION_RECORD *record = info ? info->ExceptionRecord : nullptr;
        if (!record || record->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
        const bool wide = record->ExceptionCode == 0x4001000Aul;
        if (!wide && record->ExceptionCode != 0x40010006ul) return EXCEPTION_CONTINUE_SEARCH;
        const size_t most = static_cast<size_t>(record->ExceptionInformation[0]);
        std::string line;
        if (wide)
        {
            const wchar_t *text = reinterpret_cast<const wchar_t *>(record->ExceptionInformation[1]);
            for (size_t i = 0; text && i < most && i < 2048 && text[i]; ++i) line.push_back(text[i] < 0x80 ? static_cast<char>(text[i]) : '?');
        }
        else
        {
            const char *text = reinterpret_cast<const char *>(record->ExceptionInformation[1]);
            for (size_t i = 0; text && i < most && i < 2048 && text[i]; ++i) line.push_back(text[i]);
        }
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (!line.empty() && line != lastDebug)
        {
            lastDebug = line;
            fake::Note("debug: " + line);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    bool Named(const std::wstring &file)
    {
        const std::wstring head = L"Stalker2-";
        const std::wstring tail = L"-Shipping.exe";
        if (file.size() <= head.size() + tail.size()) return false;
        return _wcsnicmp(file.c_str(), head.c_str(), head.size()) == 0 && _wcsicmp(file.c_str() + file.size() - tail.size(), tail.c_str()) == 0;
    }

    std::string Escape(const std::string &text)
    {
        std::string out;
        for (const char c : text)
        {
            if (c == '"' || c == '\\') out += '\\';
            if (static_cast<unsigned char>(c) < 0x20) continue;
            out += c;
        }
        return out;
    }

    int overlays = 0;
    int pages = 0;
    int opens = 0;
    int made = 0;
    int dropped = 0;
    int deviceDropped = 0;
    int pipelineInit = 0;
    int pipelineBind = 0;
    int pipelineDestroy = 0;

    alignas(16) unsigned char deviceStore[64]{};
    alignas(16) unsigned char listStore[64]{};

    reshade::api::device *FakeDevice()
    {
        return reinterpret_cast<reshade::api::device *>(deviceStore);
    }

    reshade::api::command_list *FakeList()
    {
        return reinterpret_cast<reshade::api::command_list *>(listStore);
    }

    HWND Window()
    {
        WNDCLASSEXW kind{};
        kind.cbSize = sizeof(kind);
        kind.lpfnWndProc = DefWindowProcW;
        kind.hInstance = GetModuleHandleW(nullptr);
        kind.lpszClassName = L"UnrealWindow";
        RegisterClassExW(&kind);
        return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"UnrealWindow", L"fake host", WS_POPUP | WS_VISIBLE, -32000, -32000, 64, 64, nullptr, nullptr, kind.hInstance, nullptr);
    }

    void Pump()
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    std::vector<std::pair<int, void *>> Snapshot()
    {
        std::lock_guard<std::mutex> hold(fake::gate);
        return fake::live;
    }

    void Fire(reshade::addon_event wanted, reshade::api::effect_runtime *runtime, int &count)
    {
        using Event = void (*)(reshade::api::effect_runtime *);
        for (const auto &[ev, fn] : Snapshot())
            if (ev == static_cast<int>(wanted))
            {
                ++count;
                reinterpret_cast<Event>(fn)(runtime);
            }
    }

    void FireDevice(int &count)
    {
        using Event = void (*)(reshade::api::device *);
        for (const auto &[ev, fn] : Snapshot())
            if (ev == static_cast<int>(reshade::addon_event::destroy_device))
            {
                ++count;
                reinterpret_cast<Event>(fn)(FakeDevice());
            }
    }

    void FirePipeline()
    {
        using Init = void (*)(reshade::api::device *, reshade::api::pipeline_layout, uint32_t, const reshade::api::pipeline_subobject *, reshade::api::pipeline);
        using Bind = void (*)(reshade::api::command_list *, reshade::api::pipeline_stage, reshade::api::pipeline);
        using Gone = void (*)(reshade::api::device *, reshade::api::pipeline);
        const reshade::api::pipeline_layout layout{ 0x100 };
        const reshade::api::pipeline pipeline{ 0x5afe5afeull };
        for (const auto &[ev, fn] : Snapshot())
        {
            if (ev == static_cast<int>(reshade::addon_event::init_pipeline))
            {
                ++pipelineInit;
                reinterpret_cast<Init>(fn)(FakeDevice(), layout, 0, nullptr, pipeline);
            }
            else if (ev == static_cast<int>(reshade::addon_event::bind_pipeline))
            {
                ++pipelineBind;
                reinterpret_cast<Bind>(fn)(FakeList(), reshade::api::pipeline_stage::all_graphics, pipeline);
            }
            else if (ev == static_cast<int>(reshade::addon_event::destroy_pipeline))
            {
                ++pipelineDestroy;
                reinterpret_cast<Gone>(fn)(FakeDevice(), pipeline);
            }
        }
    }

    void Write(const std::filesystem::path &file, const std::string &status, bool second, long long ms)
    {
        std::lock_guard<std::mutex> hold(fake::gate);
        std::ofstream out(file, std::ios::binary);
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        std::string exeName;
        for (const wchar_t c : std::filesystem::path(self).filename().wstring()) exeName.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        out << "{\n  \"status\": \"" << status << "\",\n  \"exe_name\": \"" << Escape(exeName) << "\",\n  \"overlay_title\": \"" << Escape(fake::overlayTitle) << "\",\n  \"second_copy_refused\": " << (second ? "true" : "false")
            << ",\n  \"imgui_calls\": " << fake::imguiCalls.load() << ",\n  \"imgui_calls_load\": " << fake::byPhase[static_cast<int>(fake::Phase::Load)].load()
            << ",\n  \"imgui_calls_present\": " << fake::byPhase[static_cast<int>(fake::Phase::Present)].load() << ",\n  \"imgui_calls_overlay\": " << fake::byPhase[static_cast<int>(fake::Phase::Overlay)].load()
            << ",\n  \"imgui_calls_open\": " << fake::byPhase[static_cast<int>(fake::Phase::Open)].load() << ",\n  \"overlay_events\": " << overlays << ",\n  \"page_calls\": " << pages
            << ",\n  \"open_events\": " << opens << ",\n  \"runtime_made\": " << made << ",\n  \"runtime_dropped\": " << dropped << ",\n  \"device_dropped\": " << deviceDropped
            << ",\n  \"pipeline_init\": " << pipelineInit << ",\n  \"pipeline_bind\": " << pipelineBind << ",\n  \"pipeline_destroy\": " << pipelineDestroy
            << ",\n  \"exit_thread\": " << GetCurrentThreadId() << ",\n  \"main_thread\": " << fake::mainThread << ",\n  \"runtime_traps\": " << fake::traps.size() << ",\n  \"elapsed_ms\": " << ms << ",\n  \"imgui_used\": {";
        bool comma = false;
        for (int i = 0; i < 512; ++i)
        {
            if (!fake::byName[i] || !fake::names[i]) continue;
            out << (comma ? ", " : "") << "\"" << fake::names[i] << "\": " << fake::byName[i];
            comma = true;
        }
        out << "},\n  \"journal\": [\n";
        for (size_t i = 0; i < fake::journal.size(); ++i) out << "    \"" << Escape(fake::journal[i]) << "\"" << (i + 1 < fake::journal.size() ? "," : "") << "\n";
        out << "  ],\n  \"traps\": [\n";
        for (size_t i = 0; i < fake::traps.size(); ++i) out << "    \"" << Escape(fake::traps[i]) << "\"" << (i + 1 < fake::traps.size() ? "," : "") << "\n";
        out << "  ]\n}\n";
    }

    struct Leaving
    {
        std::filesystem::path out;
        std::string status;
        bool refused = false;
        long long ms = 0;
    };

    Leaving leaving;

    DWORD WINAPI Bail(LPVOID)
    {
        Write(leaving.out, leaving.status, leaving.refused, leaving.ms);
        std::printf("fake host %s: exit(777004) from worker thread %lu\n", leaving.status.c_str(), GetCurrentThreadId());
        std::fflush(stdout);
        std::exit(777004);
    }
}

int wmain(int argc, wchar_t **argv)
{
    if (argc < 4)
    {
        std::printf("usage: Stalker2-<build>-Shipping.exe <addon> <sandbox> <out.json> [presents] [free|exit|kill|live|worker|crash|quit]\n");
        return 2;
    }
    const std::wstring mode = argc > 5 ? std::wstring(argv[5]) : std::wstring(L"free");
    if (mode != L"free" && mode != L"exit" && mode != L"kill" && mode != L"live" && mode != L"worker" && mode != L"crash" && mode != L"quit")
    {
        std::printf("the mode is free, exit, kill, live, worker, crash or quit\n");
        return 2;
    }
    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::filesystem::path selfPath(self);
    if (!Named(selfPath.filename().wstring()) || selfPath.parent_path().filename() != L"bin")
    {
        std::printf("the fake host runs only as bin\\Stalker2-<build>-Shipping.exe\n");
        return 2;
    }
    AddVectoredExceptionHandler(1, Debugged);
    if (!Alone())
    {
        std::printf("a real game process is running, the fake host refuses to start\n");
        return 3;
    }
    fake::mainThread = GetCurrentThreadId();
    fake::Fill(fake::table);
    const std::filesystem::path addon(argv[1]);
    const std::filesystem::path sandbox(argv[2]);
    const std::filesystem::path out(argv[3]);
    const int presents = argc > 4 ? _wtoi(argv[4]) : 300;
    std::error_code code;
    std::filesystem::create_directories(sandbox, code);
    SetEnvironmentVariableW(L"LOCALAPPDATA", sandbox.c_str());
    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [started] { return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count()); };
    std::thread([out, elapsed] {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        Write(out, "timeout", false, elapsed());
        ExitProcess(4);
    }).detach();
    const HWND window = Window();
    fake::Note(window ? "game window UnrealWindow made on the main thread" : "game window refused " + std::to_string(GetLastError()));
    fake::Note("load " + addon.filename().string());
    const HMODULE module = LoadLibraryW(addon.c_str());
    fake::Note(module ? "loaded" : "load refused " + std::to_string(GetLastError()));
    const std::filesystem::path copy = sandbox / "second-copy.addon64";
    std::filesystem::copy_file(addon, copy, std::filesystem::copy_options::overwrite_existing, code);
    const int before = fake::registers.load();
    const HMODULE twin = LoadLibraryW(copy.c_str());
    const bool refused = twin == nullptr && fake::registers.load() == before;
    fake::Note(twin ? "second copy loaded" : "second copy refused");
    if (twin) FreeLibrary(twin);
    reshade::api::FakeRuntime runtime;
    using Event = void (*)(reshade::api::effect_runtime *);
    using Open = bool (*)(reshade::api::effect_runtime *, bool, reshade::api::input_source);
    wchar_t stallText[32]{};
    const int stallMs = GetEnvironmentVariableW(L"ZSDK_FAKE_STALL", stallText, 32) ? _wtoi(stallText) : 0;
    ULONGLONG stallUntil = 0;
    bool stalled = false;
    if (module) Fire(reshade::addon_event::init_effect_runtime, &runtime, made);
    for (int i = 0; i < presents && module; ++i)
    {
        if (stallMs > 0 && i == (presents * 7) / 10 && !stallUntil)
        {
            stallUntil = GetTickCount64() + static_cast<ULONGLONG>(stallMs);
            stalled = true;
            fake::Note("stall: the window thread pumps no messages for " + std::to_string(stallMs) + " ms while presents go on");
        }
        if (stalled && GetTickCount64() >= stallUntil)
        {
            stalled = false;
            fake::Note("stall: the window thread pumps again");
        }
        if (mode == L"quit" && i == presents / 4)
        {
            PostQuitMessage(0);
            fake::Note("quit: PostQuitMessage on the main thread, the host keeps presenting and pumping");
        }
        if (!stalled) Pump();
        const std::vector<std::pair<int, void *>> calls = Snapshot();
        ++fake::frames;
        fake::phase = static_cast<int>(fake::Phase::Present);
        for (const auto &[ev, fn] : calls)
            if (ev == static_cast<int>(reshade::addon_event::reshade_present)) reinterpret_cast<Event>(fn)(&runtime);
        if (i == 2) FirePipeline();
        fake::phase = static_cast<int>(fake::Phase::Overlay);
        for (const auto &[ev, fn] : calls)
            if (ev == static_cast<int>(reshade::addon_event::reshade_overlay))
            {
                ++overlays;
                reinterpret_cast<Event>(fn)(&runtime);
            }
        if (fake::overlayCallback)
        {
            ++pages;
            reinterpret_cast<Event>(fake::overlayCallback)(&runtime);
        }
        if (i == presents / 3 || i == (presents * 2) / 3)
        {
            fake::phase = static_cast<int>(fake::Phase::Open);
            for (const auto &[ev, fn] : calls)
                if (ev == static_cast<int>(reshade::addon_event::reshade_open_overlay))
                {
                    ++opens;
                    reinterpret_cast<Open>(fn)(&runtime, i == presents / 3, reshade::api::input_source::keyboard);
                }
        }
        fake::phase = static_cast<int>(fake::Phase::Load);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Pump();
    fake::Note("presents " + std::to_string(presents));
    if (mode == L"kill")
    {
        fake::Note("kill: TerminateProcess, no DllMain runs");
        Write(out, "kill", refused, elapsed());
        std::printf("fake host kill: TerminateProcess(9)\n");
        std::fflush(stdout);
        TerminateProcess(GetCurrentProcess(), 9);
        return 9;
    }
    if (mode == L"live")
    {
        fake::Note("live: wmain returns while the swap chain still draws, no destroy_effect_runtime");
        Write(out, "live", refused, elapsed());
        std::printf("fake host live: wmain returns with the swap chain alive\n");
        return 0;
    }
    if (mode == L"crash" || mode == L"worker")
    {
        if (mode == L"worker")
        {
            Fire(reshade::addon_event::destroy_effect_runtime, &runtime, dropped);
            FireDevice(deviceDropped);
            fake::Note("worker: destroy_effect_runtime and destroy_device, then exit() from a worker thread");
        }
        else fake::Note("crash: exit() from a worker thread while the swap chain still draws, as a UE crash handler that crashed");
        leaving.out = out;
        leaving.status = mode == L"worker" ? "worker" : "crash";
        leaving.refused = refused;
        leaving.ms = elapsed();
        const HANDLE worker = CreateThread(nullptr, 0, Bail, nullptr, 0, nullptr);
        if (worker) WaitForSingleObject(worker, INFINITE);
        return 5;
    }
    Fire(reshade::addon_event::destroy_effect_runtime, &runtime, dropped);
    FireDevice(deviceDropped);
    if (window) DestroyWindow(window);
    Pump();
    if (mode == L"exit" || mode == L"quit")
    {
        fake::Note("exit: destroy_effect_runtime and destroy_device, then wmain returns without FreeLibrary, the CRT calls exit and ExitProcess on the main thread");
        Write(out, mode == L"quit" ? "quit" : "exit", refused, elapsed());
        std::printf("fake host exit: %d imgui calls (%d present, %d overlay, %d open), %d pages, %zu traps\n", fake::imguiCalls.load(), fake::byPhase[1].load(), fake::byPhase[2].load(), fake::byPhase[3].load(), pages, fake::traps.size());
        return 0;
    }
    if (module)
    {
        FreeLibrary(module);
        fake::Note("freed");
    }
    Write(out, "done", refused, elapsed());
    std::printf("fake host done: %d imgui calls (%d present, %d overlay, %d open), %d pages, %zu traps\n", fake::imguiCalls.load(), fake::byPhase[1].load(), fake::byPhase[2].load(), fake::byPhase[3].load(), pages, fake::traps.size());
    return 0;
}
