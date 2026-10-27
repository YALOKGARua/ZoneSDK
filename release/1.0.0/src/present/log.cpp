#include <zsdk/present.h>

#include "../detail/runtime.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zsdk
{
    namespace
    {
        constexpr uintmax_t kLogMax = 8u << 20;
        constexpr size_t kPendingMax = 4096;
        constexpr uint64_t kFlushMs = 500;
        constexpr uint64_t kRareMs = 60000;
        constexpr size_t kHushesKept = 512;

        struct Hush
        {
            uint64_t at = 0;
            int muted = 0;
        };

        std::mutex hushGate;
        std::unordered_map<std::string, Hush> hushes;

        std::mutex logGate;
        std::mutex flushGate;
        std::deque<std::string> pending;
        size_t dropped = 0;
        size_t droppedTotal = 0;
        std::filesystem::path logPath;
        std::filesystem::path rolledPath;
        std::atomic<int> level{ 1 };
        std::atomic<uint64_t> lastFlush{ 0 };
        std::atomic<bool> urgent{ false };
        std::atomic<bool> exiting{ false };

        std::unique_lock<std::mutex> Take(std::mutex &gate)
        {
            if (exiting.load()) return std::unique_lock<std::mutex>(gate, std::try_to_lock);
            return std::unique_lock<std::mutex>(gate);
        }

        void Push(const char *text)
        {
            SYSTEMTIME t{};
            GetLocalTime(&t);
            char head[48]{};
            _snprintf_s(head, sizeof(head), _TRUNCATE, "%02u:%02u:%02u.%03u [%lu] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId());
            const std::unique_lock<std::mutex> hold = Take(logGate);
            if (!hold.owns_lock()) return;
            if (pending.size() >= kPendingMax)
            {
                pending.pop_front();
                ++dropped;
                ++droppedTotal;
            }
            pending.push_back(std::string(head) + text);
        }

        std::string Utf8(const std::wstring &wide)
        {
            if (wide.empty()) return {};
            const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            if (size <= 0) return {};
            std::string out(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size, nullptr, nullptr);
            return out;
        }

        std::string Variable(const wchar_t *name)
        {
            wchar_t buffer[1024]{};
            const DWORD size = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
            if (!size || size >= std::size(buffer)) return {};
            std::wstring text(buffer, size);
            while (!text.empty() && (text.back() == L'\\' || text.back() == L'/')) text.pop_back();
            return Utf8(text);
        }

        char Fold(char c)
        {
            if (c >= 'A' && c <= 'Z') return static_cast<char>(c + 32);
            return c == '/' ? '\\' : c;
        }

        bool Word(char c)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || u == '_' || u >= 0x80;
        }

        bool Same(std::string_view text, size_t at, std::string_view what)
        {
            if (what.empty() || at + what.size() > text.size()) return false;
            for (size_t i = 0; i < what.size(); ++i)
                if (Fold(text[at + i]) != Fold(what[i])) return false;
            return true;
        }

        std::string Swap(std::string_view text, std::string_view root, std::string_view shown)
        {
            if (root.size() < 3) return std::string(text);
            std::string out;
            size_t i = 0;
            while (i < text.size())
            {
                const size_t end = i + root.size();
                if (Same(text, i, root) && (i == 0 || !Word(text[i - 1])) && (end >= text.size() || !(Word(text[end]) || text[end] == '-' || text[end] == '.')))
                {
                    out += shown;
                    i = end;
                    continue;
                }
                out += text[i++];
            }
            return out;
        }

        std::string Hide(std::string_view text, std::string_view user)
        {
            if (user.size() < 2) return std::string(text);
            std::string out;
            size_t i = 0;
            while (i < text.size())
            {
                const size_t end = i + 1 + user.size();
                const bool after = end >= text.size() || text[end] == '\\' || text[end] == '/' || text[end] == '"' || text[end] == '\'' || text[end] == ' ' || text[end] == '\t' || text[end] == '\r' || text[end] == '\n';
                if ((text[i] == '\\' || text[i] == '/') && Same(text, i + 1, user) && after)
                {
                    out += text[i];
                    out += "<user>";
                    i = end;
                    continue;
                }
                out += text[i++];
            }
            return out;
        }
    }

    void Log(const char *shape, ...)
    {
        char buffer[4096]{};
        va_list args;
        va_start(args, shape);
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, shape, args);
        va_end(args);
        Push(buffer);
    }

    void Note(int wanted, const char *shape, ...)
    {
        if (wanted > level.load()) return;
        char buffer[4096]{};
        va_list args;
        va_start(args, shape);
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, shape, args);
        va_end(args);
        Push(buffer);
    }

    void Rare(const char *tag, const char *shape, ...)
    {
        const uint64_t now = GetTickCount64();
        int muted = 0;
        {
            const std::unique_lock<std::mutex> hold = Take(hushGate);
            if (!hold.owns_lock()) return;
            const std::string key = tag ? tag : "";
            if (hushes.size() >= kHushesKept && hushes.find(key) == hushes.end()) hushes.clear();
            Hush &hush = hushes[key];
            if (hush.at && Age(now, hush.at) < kRareMs)
            {
                ++hush.muted;
                return;
            }
            muted = hush.muted;
            hush.at = now;
            hush.muted = 0;
        }
        char buffer[4096]{};
        va_list args;
        va_start(args, shape);
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, shape, args);
        va_end(args);
        if (muted > 0)
        {
            char tail[96]{};
            _snprintf_s(tail, sizeof(tail), _TRUNCATE, " (repeated %d times since the last such line)", muted);
            strncat_s(buffer, sizeof(buffer), tail, _TRUNCATE);
        }
        Push(buffer);
    }

    void Flush()
    {
        const std::unique_lock<std::mutex> once = Take(flushGate);
        if (!once.owns_lock()) return;
        std::deque<std::string> lines;
        std::filesystem::path path;
        std::filesystem::path rolled;
        size_t lost = 0;
        {
            const std::unique_lock<std::mutex> hold = Take(logGate);
            if (!hold.owns_lock() || logPath.empty() || pending.empty()) return;
            lines.swap(pending);
            path = logPath;
            rolled = rolledPath;
            lost = dropped;
            dropped = 0;
        }
        {
            std::ofstream file(path, std::ios::binary | std::ios::app);
            if (!file) return;
            if (lost) file << "log dropped " << lost << " lines\n";
            for (const std::string &line : lines) file << line << '\n';
        }
        std::error_code code;
        const uintmax_t size = std::filesystem::file_size(path, code);
        if (!code && size > kLogMax) log::Roll(path, rolled);
    }

    void Urgent()
    {
        urgent = true;
    }

    namespace log
    {
        void Roll(const std::filesystem::path &file, const std::filesystem::path &rolled)
        {
            if (file.empty() || rolled.empty()) return;
            MoveFileExW(file.c_str(), rolled.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        void Open(const std::filesystem::path &file, const std::filesystem::path &rolled)
        {
            std::lock_guard<std::mutex> hold(logGate);
            logPath = file;
            rolledPath = rolled;
        }

        std::filesystem::path Path()
        {
            std::lock_guard<std::mutex> hold(logGate);
            return logPath;
        }

        int Level()
        {
            return level.load();
        }

        void SetLevel(int wanted)
        {
            level = wanted;
        }

        void Hushed(uint64_t now)
        {
            std::vector<std::pair<std::string, int>> closed;
            {
                std::lock_guard<std::mutex> hold(hushGate);
                for (auto &[tag, hush] : hushes)
                    if (hush.muted > 0 && Age(now, hush.at) >= kRareMs)
                    {
                        closed.emplace_back(tag, hush.muted);
                        hush.muted = 0;
                    }
            }
            for (const auto &[tag, muted] : closed)
            {
                char line[256]{};
                _snprintf_s(line, sizeof(line), _TRUNCATE, "%s: repeated %d more times since the last line", tag.c_str(), muted);
                Push(line);
            }
        }

        bool Due(uint64_t now)
        {
            const bool rush = urgent.exchange(false);
            if (!rush && Age(now, lastFlush.load()) < kFlushMs) return false;
            lastFlush = now;
            return true;
        }

        size_t Pending()
        {
            std::lock_guard<std::mutex> hold(logGate);
            return pending.size();
        }

        size_t Dropped()
        {
            std::lock_guard<std::mutex> hold(logGate);
            return droppedTotal;
        }

        std::string Masked(std::string_view utf8)
        {
            const std::string local = Variable(L"LOCALAPPDATA");
            const std::string profile = Variable(L"USERPROFILE");
            std::string out(utf8);
            if (local.size() >= profile.size())
            {
                out = Swap(out, local, "%LOCALAPPDATA%");
                out = Swap(out, profile, "%USERPROFILE%");
            }
            else
            {
                out = Swap(out, profile, "%USERPROFILE%");
                out = Swap(out, local, "%LOCALAPPDATA%");
            }
            return Hide(out, Variable(L"USERNAME"));
        }

        std::string Shown(const std::filesystem::path &path)
        {
            const std::u8string text = path.u8string();
            return Masked(std::string_view(reinterpret_cast<const char *>(text.data()), text.size()));
        }
    }

    namespace detail
    {
        void Exit(bool on)
        {
            exiting = on;
        }

        bool Exiting()
        {
            return exiting.load();
        }

#if defined(ZSDK_TESTS)
        void Grip(int which, bool hold)
        {
            std::mutex &gate = which == 0 ? logGate : which == 1 ? hushGate : flushGate;
            if (hold) gate.lock();
            else gate.unlock();
        }
#endif
    }
}
