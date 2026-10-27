#include <zsdk/present.h>

#include "../detail/runtime.h"
#include "zsdk_build.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zsdk::report
{
    namespace
    {
        std::mutex gate;
        std::wstring folderOverride;

        std::string Utf8(const std::wstring &wide)
        {
            if (wide.empty()) return {};
            const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            if (size <= 0) return {};
            std::string out(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size, nullptr, nullptr);
            return out;
        }

        std::filesystem::path ExeFolder()
        {
            {
                std::lock_guard<std::mutex> hold(gate);
                if (!folderOverride.empty()) return folderOverride;
            }
            wchar_t path[MAX_PATH]{};
            const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
            if (!size || size >= MAX_PATH) return {};
            return std::filesystem::path(path).parent_path();
        }

        const char *PhaseName(Phase phase)
        {
            switch (phase)
            {
            case Phase::Off: return "off";
            case Phase::Hooking: return "hooking";
            case Phase::Binding: return "binding";
            case Phase::Wiring: return "wiring";
            case Phase::Waiting: return "waiting";
            case Phase::Ready: return "ready";
            case Phase::Failed: return "failed";
            case Phase::Halted: return "halted";
            case Phase::Lost: return "lost";
            default: return "?";
            }
        }

        std::string Yes(bool value)
        {
            return value ? "yes" : "no";
        }

        std::string Joined(const std::vector<std::string> &list)
        {
            std::string out;
            for (const std::string &one : list) out += (out.empty() ? "" : ", ") + one;
            return out.empty() ? std::string("-") : out;
        }

        void Line(std::string &out, const std::string &text)
        {
            out += text;
            out += "\r\n";
        }

        std::vector<std::string> Addons(const std::filesystem::path &folder)
        {
            std::vector<std::string> out;
            if (folder.empty()) return out;
            WIN32_FIND_DATAW found{};
            const HANDLE walk = FindFirstFileW((folder / L"*.addon64").c_str(), &found);
            if (walk == INVALID_HANDLE_VALUE) return out;
            do
            {
                if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
                const unsigned long long bytes = (static_cast<unsigned long long>(found.nFileSizeHigh) << 32) | found.nFileSizeLow;
                char size[32]{};
                _snprintf_s(size, sizeof(size), _TRUNCATE, ", %llu bytes", bytes);
                out.push_back(Utf8(found.cFileName) + size);
            } while (FindNextFileW(walk, &found) && out.size() < 64);
            FindClose(walk);
            std::sort(out.begin(), out.end());
            return out;
        }

        std::vector<std::string> Tail(const std::filesystem::path &file, size_t lines)
        {
            std::vector<std::string> out;
            if (file.empty()) return out;
            std::ifstream in(file, std::ios::binary | std::ios::ate);
            if (!in) return out;
            const std::streamoff size = in.tellg();
            const std::streamoff from = size > static_cast<std::streamoff>(kTailBytes) ? size - static_cast<std::streamoff>(kTailBytes) : 0;
            in.seekg(from);
            std::string body(static_cast<size_t>(size - from), '\0');
            in.read(body.data(), static_cast<std::streamsize>(body.size()));
            body.resize(static_cast<size_t>(in.gcount()));
            size_t at = 0;
            if (from > 0)
            {
                const size_t first = body.find('\n');
                at = first == std::string::npos ? body.size() : first + 1;
            }
            while (at < body.size())
            {
                size_t end = body.find('\n', at);
                if (end == std::string::npos) end = body.size();
                std::string one = body.substr(at, end - at);
                if (!one.empty() && one.back() == '\r') one.pop_back();
                out.push_back(std::move(one));
                at = end + 1;
            }
            if (out.size() > lines) out.erase(out.begin(), out.end() - static_cast<std::ptrdiff_t>(lines));
            return out;
        }

        std::string ExeLine()
        {
            const std::string name = exe::Name();
            if (!exe::Steam()) return name + ", not the Steam build, game calls stay off";
            const std::string sha = exe::Sha();
            if (sha.empty()) return name + ", not hashed yet";
            const guard::Build *build = exe::Build();
            return name + ", sha256 " + sha + (build ? std::string(", known build ") + build->id : std::string(", unknown build"));
        }
    }

    std::string Version(void *module)
    {
        if (!module) return "unknown";
        const HMODULE image = static_cast<HMODULE>(module);
        const HRSRC found = FindResourceW(image, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16));
        if (!found) return "unknown";
        const HGLOBAL held = LoadResource(image, found);
        const DWORD size = SizeofResource(image, found);
        const auto *blob = static_cast<const unsigned char *>(held ? LockResource(held) : nullptr);
        constexpr wchar_t kKey[] = L"VS_VERSION_INFO";
        constexpr size_t kFixed = (6 + sizeof(kKey) + 3) & ~static_cast<size_t>(3);
        if (!blob || size < kFixed + 4 * sizeof(DWORD)) return "unknown";
        WORD valueSize = 0;
        std::memcpy(&valueSize, blob + 2, sizeof(valueSize));
        if (valueSize < 13 * sizeof(DWORD) || std::memcmp(blob + 6, kKey, sizeof(kKey)) != 0) return "unknown";
        DWORD high = 0;
        DWORD low = 0;
        std::memcpy(&high, blob + kFixed + 2 * sizeof(DWORD), sizeof(high));
        std::memcpy(&low, blob + kFixed + 3 * sizeof(DWORD), sizeof(low));
        char text[48]{};
        _snprintf_s(text, sizeof(text), _TRUNCATE, "%lu.%lu.%lu.%lu", static_cast<unsigned long>(high >> 16), static_cast<unsigned long>(high & 0xFFFF), static_cast<unsigned long>(low >> 16),
                    static_cast<unsigned long>(low & 0xFFFF));
        return text;
    }

    std::filesystem::path File()
    {
        const std::filesystem::path folder = ini::Folder();
        return folder.empty() ? std::filesystem::path() : folder / L"report.txt";
    }

    std::string Text(void *reshade)
    {
        const App &a = app::Get();
        const Identity &id = a.identity;
        Flush();
        std::string out;
        Line(out, std::string("ZoneSDK report, ") + ini::Stamp() + " local time");
        Line(out, std::string("add-on: ") + (id.name ? id.name : "?") + " " + (id.version ? id.version : "?") + ", tag " + (id.tag ? id.tag : "?") + ", build " + ledger::Addon());
        Line(out, std::string("zsdk: ") + ZSDK_VERSION + " manifest " + ZSDK_MANIFEST);
        Line(out, "game exe: " + ExeLine());
        Line(out, "reshade: " + Version(reshade));
        const world::Grip w = world::Snapshot();
        Line(out, std::string("state: ") + PhaseName(thread::State(w.stable)) + ", hooked " + Yes(thread::Hooked()) + ", bound " + Yes(thread::Bound()) + ", wired " + Yes(thread::Wired()) + ", rejected " +
                      Yes(thread::Rejected()) + ", halted " + Yes(thread::Halted()) + ", language " + lang::Code(lang::Current()));
        const verbs::Report r = verbs::Last();
        Line(out, "functions: required " + std::to_string(r.requiredOk) + "/" + std::to_string(r.requiredAll) + ", optional " + std::to_string(r.optionalOk) + "/" + std::to_string(r.optionalAll) +
                      (r.missing.empty() ? std::string() : ", missing " + r.missing));
        const std::vector<std::string> addons = Addons(ExeFolder());
        Line(out, "add-ons next to the game exe: " + std::to_string(addons.size()));
        for (const std::string &one : addons) Line(out, "  " + one);
        const ini::Kept kept = ini::Ledger();
        Line(out, "[safety]");
        Line(out, "session " + (kept.session.empty() ? std::string("-") : kept.session) + ", last run " + (ledger::Last().empty() ? std::string("-") : ledger::Last()) + ", ended cleanly " + Yes(ledger::CleanStart()));
        Line(out, "pending " + (kept.pending.empty() ? std::string("-") : kept.pending));
        Line(out, "disabled " + Joined(kept.disabled));
        Line(out, "watch " + Joined(kept.watch));
        Line(out, "disabled at boot " + Joined(ledger::Booted()));
        Line(out, "proven " + std::to_string(kept.proven.size()) + " keys, preloaded " + std::to_string(ledger::PreloadCount()));
        if (const auto more = hooks::Get().report)
        {
            Line(out, std::string("[") + (id.name ? id.name : "add-on") + "]");
            try
            {
                std::string own = more();
                while (!own.empty() && (own.back() == '\n' || own.back() == '\r')) own.pop_back();
                Line(out, own);
            }
            catch (const std::exception &error)
            {
                Line(out, std::string("the add-on section failed: ") + error.what());
            }
        }
        const std::filesystem::path logFile = ini::LogPath();
        const std::vector<std::string> lines = Tail(logFile, kLogLines);
        Line(out, "[log] last " + std::to_string(lines.size()) + " lines of " + log::Shown(logFile));
        for (const std::string &one : lines) Line(out, one);
        return log::Masked(out);
    }

    bool Save(const std::string &text)
    {
        const std::filesystem::path file = File();
        if (file.empty()) return false;
        const bool ok = ini::WriteAtomic(file, text);
        if (ok) Log("report saved to %s, %zu bytes", log::Shown(file).c_str(), text.size());
        else Log("report could not be saved to %s", log::Shown(file).c_str());
        return ok;
    }
}

#if defined(ZSDK_TESTS)
namespace zsdk::detail
{
    void ReportFolder(const wchar_t *folder)
    {
        std::lock_guard<std::mutex> hold(zsdk::report::gate);
        zsdk::report::folderOverride = folder ? folder : L"";
    }
}
#endif
