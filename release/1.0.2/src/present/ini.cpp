#include <zsdk/present.h>

#include "../detail/ledger_raw.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace zsdk::ini
{
    namespace
    {
        constexpr uintmax_t kLogStart = 4u << 20;
        constexpr uint64_t kSaveDelayMs = 1000;
        constexpr uint64_t kSaveBackoffMs = 60000;

        struct Box
        {
            const Schema *schema = nullptr;
            void *data = nullptr;

            Box() = default;

            explicit Box(const Schema &s) : schema(&s), data(s.make())
            {
            }

            Box(const Box &) = delete;
            Box &operator=(const Box &) = delete;

            ~Box()
            {
                if (schema && data) schema->drop(data);
            }
        };

        struct Slip
        {
            char body[ledger::kMarkMost]{};
            DWORD size = 0;
        };

        std::mutex gate;
        std::mutex fileGate;
        std::filesystem::path folder;
        std::unique_ptr<Box> settings;
        Kept kept;
        std::vector<std::string> booted;
        std::vector<std::string> closed;
        std::string inflight;
        std::string previous;
        std::atomic<uint64_t> revision{ 1 };
        std::atomic<bool> cleanStart{ false };
        std::atomic<bool> stuck{ false };
        std::atomic<bool> frozen{ false };
        std::atomic<int> slipAt{ -1 };
        Slip slips[2]{};
        wchar_t markPath[1024]{};
        wchar_t reloadName[96]{};
        bool dirty = false;
        uint64_t changedAt = 0;
        uint64_t saveDelay = kSaveDelayMs;

        const Schema &Need()
        {
            const Schema *schema = app::Get().schema;
            if (!schema || !schema->visit || !schema->make) throw std::logic_error("the add-on has no settings schema");
            return *schema;
        }

        Box &Held()
        {
            if (!settings) settings = std::make_unique<Box>(Need());
            return *settings;
        }

        std::string Trim(std::string_view raw)
        {
            size_t from = 0, to = raw.size();
            while (from < to && (raw[from] == ' ' || raw[from] == '\t' || raw[from] == '\r' || raw[from] == '\n')) ++from;
            while (to > from && (raw[to - 1] == ' ' || raw[to - 1] == '\t' || raw[to - 1] == '\r' || raw[to - 1] == '\n')) --to;
            return std::string(raw.substr(from, to - from));
        }

        std::string Lower(std::string_view raw)
        {
            std::string out(raw);
            for (char &c : out)
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            return out;
        }

        std::string Plain(std::string_view raw, size_t most)
        {
            std::string out;
            for (const char c : raw)
                if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7F) out += c;
            const size_t cap = std::min(most, Limits::kTextMost);
            if (out.size() <= cap) return out;
            size_t cut = cap;
            while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
            out.resize(cut);
            return out;
        }

        bool Number(const std::string &text, double &out)
        {
            if (text.empty()) return false;
            char *end = nullptr;
            const double value = std::strtod(text.c_str(), &end);
            if (end == text.c_str() || *end != '\0' || !std::isfinite(value)) return false;
            out = value;
            return true;
        }

        bool Whole(const std::string &text, int &out)
        {
            double value = 0;
            if (!Number(text, value) || value < -1.0e9 || value > 1.0e9) return false;
            out = static_cast<int>(std::lround(value));
            return true;
        }

        bool Flag(const std::string &text, bool &out)
        {
            const std::string low = Lower(text);
            if (low == "1" || low == "true" || low == "yes" || low == "on")
            {
                out = true;
                return true;
            }
            if (low == "0" || low == "false" || low == "no" || low == "off")
            {
                out = false;
                return true;
            }
            return false;
        }

        std::string On(bool value)
        {
            return value ? "1" : "0";
        }

        double Clamp(double value, double low, double high, double fallback)
        {
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        }

        std::vector<std::string> List(const std::string &text)
        {
            std::vector<std::string> out;
            size_t from = 0;
            while (from <= text.size())
            {
                size_t comma = text.find(',', from);
                if (comma == std::string::npos) comma = text.size();
                const std::string one = Trim(std::string_view(text).substr(from, comma - from));
                if (!one.empty() && std::find(out.begin(), out.end(), one) == out.end()) out.push_back(one);
                from = comma + 1;
            }
            return out;
        }

        std::string Joined(const std::vector<std::string> &list)
        {
            std::string out;
            for (const std::string &one : list) out += (out.empty() ? "" : ",") + one;
            return out;
        }

        bool Built(const std::string &build)
        {
            return !build.empty() && build != "?";
        }

        std::string Keep(std::vector<std::string> &list, const std::string &key)
        {
            if (std::find(list.begin(), list.end(), key) != list.end()) return {};
            std::string dropped;
            while (list.size() >= Limits::kDisabledKept)
            {
                dropped += (dropped.empty() ? "" : ",") + list.front();
                list.erase(list.begin());
            }
            list.push_back(key);
            return dropped;
        }

        const Owned *OwnedOf(const Schema &schema, const std::string &section)
        {
            for (size_t i = 0; i < schema.ownedCount; ++i)
                if (section == schema.owned[i].name) return &schema.owned[i];
            return nullptr;
        }

        class Sections final : public Visitor
        {
        public:
            std::vector<std::string> names;

            void Flag(const char *section, const char *, bool &) override { Add(section); }
            void Whole(const char *section, const char *, int &, int, int, int) override { Add(section); }
            void Real(const char *section, const char *, double &, double, double, double) override { Add(section); }
            void Word(const char *section, const char *, std::string &, const char *, const char *const *, int) override { Add(section); }
            void Pick(const char *section, const char *, int &, const char *const *, int, int) override { Add(section); }
            void Key(const char *section, const char *, keys::Chord &, keys::Chord) override { Add(section); }
            void Text(const char *section, const char *, std::string &, size_t) override { Add(section); }

        private:
            void Add(const char *section)
            {
                if (std::find(names.begin(), names.end(), section) == names.end()) names.push_back(section);
            }
        };

        class Assign final : public Visitor
        {
        public:
            Assign(const std::string &section, const std::string &key, const std::string &value) : section(section), key(key), value(value)
            {
            }

            bool known = false;

            void Flag(const char *s, const char *k, bool &v) override
            {
                if (!Mine(s, k)) return;
                bool flag = false;
                if (ini::Flag(value, flag)) v = flag;
            }

            void Whole(const char *s, const char *k, int &v, int, int, int) override
            {
                if (!Mine(s, k)) return;
                int whole = 0;
                if (ini::Whole(value, whole)) v = whole;
            }

            void Real(const char *s, const char *k, double &v, double, double, double) override
            {
                if (!Mine(s, k)) return;
                double real = 0;
                if (Number(value, real)) v = real;
            }

            void Word(const char *s, const char *k, std::string &v, const char *, const char *const *choices, int count) override
            {
                if (!Mine(s, k)) return;
                v = choices && count > 0 ? Lower(Trim(value)) : Trim(value);
            }

            void Text(const char *s, const char *k, std::string &v, size_t most) override
            {
                if (!Mine(s, k)) return;
                v = Plain(Trim(value), most);
            }

            void Pick(const char *s, const char *k, int &v, const char *const *names, int count, int) override
            {
                if (!Mine(s, k)) return;
                const std::string low = Lower(Trim(value));
                v = count;
                for (int i = 0; i < count; ++i)
                    if (low == names[i]) v = i;
            }

            void Key(const char *s, const char *k, keys::Chord &v, keys::Chord) override
            {
                if (!Mine(s, k)) return;
                const std::string low = Lower(Trim(value));
                const keys::Chord chord = keys::Parse(value);
                if (low.empty() || low == "none") v = keys::Chord{};
                else if (chord.key) v = chord;
            }

        private:
            bool Mine(const char *s, const char *k)
            {
                if (known || section != s || key != k) return false;
                known = true;
                return true;
            }

            const std::string &section;
            const std::string &key;
            const std::string &value;
        };

        class Clean final : public Visitor
        {
        public:
            void Flag(const char *, const char *, bool &) override {}

            void Whole(const char *, const char *, int &v, int, int low, int high) override
            {
                v = std::clamp(v, low, high);
            }

            void Real(const char *, const char *, double &v, double def, double low, double high) override
            {
                v = Clamp(v, low, high, def);
            }

            void Word(const char *, const char *, std::string &v, const char *def, const char *const *choices, int count) override
            {
                if (!choices || count <= 0) return;
                for (int i = 0; i < count; ++i)
                    if (v == choices[i]) return;
                v = def ? def : "";
            }

            void Pick(const char *, const char *, int &v, const char *const *, int count, int def) override
            {
                if (v < 0 || v >= count) v = def;
            }

            void Key(const char *, const char *, keys::Chord &v, keys::Chord def) override
            {
                if (v.key && !keys::Bindable(v)) v = def;
                if (!v.key) v.mods = 0;
            }

            void Text(const char *, const char *, std::string &v, size_t most) override
            {
                v = Plain(v, most);
            }
        };

        class Writer final : public Visitor
        {
        public:
            Writer(std::string &out, const Kept &keep) : out(out), keep(keep)
            {
            }

            void Flag(const char *s, const char *k, bool &v) override
            {
                Put(s, k, On(v));
            }

            void Whole(const char *s, const char *k, int &v, int, int, int) override
            {
                Put(s, k, std::to_string(v));
            }

            void Real(const char *s, const char *k, double &v, double, double, double) override
            {
                Put(s, k, ini::Real(v));
            }

            void Word(const char *s, const char *k, std::string &v, const char *def, const char *const *choices, int count) override
            {
                bool valid = !choices || count <= 0;
                for (int i = 0; !valid && i < count; ++i) valid = v == choices[i];
                Put(s, k, valid ? v : std::string(def ? def : ""));
            }

            void Pick(const char *s, const char *k, int &v, const char *const *names, int count, int def) override
            {
                Put(s, k, names[v >= 0 && v < count ? v : def]);
            }

            void Key(const char *s, const char *k, keys::Chord &v, keys::Chord) override
            {
                Put(s, k, keys::Name(v));
            }

            void Text(const char *s, const char *k, std::string &v, size_t most) override
            {
                Put(s, k, Plain(v, most));
            }

            void Finish()
            {
                if (!current.empty()) Rest(current);
            }

            void Head(const std::string &name)
            {
                if (!out.empty()) out += "\r\n";
                out += "[";
                out += name;
                out += "]\r\n";
            }

            void Line(const std::string &key, const std::string &value)
            {
                out += key;
                out += '=';
                out += value;
                out += "\r\n";
            }

            void Rest(const std::string &name)
            {
                for (const Entry &e : keep.unknown)
                    if (e.section == name) Line(e.key, e.value);
            }

        private:
            void Put(const char *s, const char *k, const std::string &value)
            {
                if (current != s)
                {
                    if (!current.empty()) Rest(current);
                    current = s;
                    Head(current);
                }
                Line(k, value);
            }

            std::string &out;
            const Kept &keep;
            std::string current;
        };

        bool KnownSection(const Schema &schema, void *probe, const std::string &section)
        {
            Sections names;
            schema.visit(probe, names);
            return std::find(names.names.begin(), names.names.end(), section) != names.names.end();
        }

        void Fell(std::vector<std::string> &disabled, const std::vector<std::string> &fallen, std::vector<std::string> &fresh, std::string &dropped)
        {
            for (const std::string &key : fallen)
            {
                const std::string gone = Keep(disabled, key);
                if (!gone.empty()) dropped += (dropped.empty() ? "" : ",") + gone;
                if (std::find(fresh.begin(), fresh.end(), key) == fresh.end()) fresh.push_back(key);
            }
        }

        bool Drop(const std::filesystem::path &file, DWORD &error)
        {
            error = 0;
            const DWORD shape = GetFileAttributesW(file.c_str());
            if (shape == INVALID_FILE_ATTRIBUTES)
            {
                error = GetLastError();
                return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
            }
            if ((shape & FILE_ATTRIBUTE_READONLY) != 0 && (shape & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                const DWORD plain = shape & ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
                SetFileAttributesW(file.c_str(), plain ? plain : FILE_ATTRIBUTE_NORMAL);
            }
            if (DeleteFileW(file.c_str())) return true;
            error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND;
        }

        std::string Fresh()
        {
            FILETIME now{};
            GetSystemTimeAsFileTime(&now);
            LARGE_INTEGER count{};
            QueryPerformanceCounter(&count);
            const unsigned long long stamp = (static_cast<unsigned long long>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
            char text[48]{};
            _snprintf_s(text, sizeof(text), _TRUNCATE, "%08lX%016llX%08lX", static_cast<unsigned long>(GetCurrentProcessId()), stamp, static_cast<unsigned long>(count.LowPart));
            return text;
        }

        std::filesystem::path ExitFile(const std::filesystem::path &base)
        {
            std::filesystem::path name(app::Get().identity.ini);
            name.replace_extension(L".exit");
            return base / name;
        }

        void Aim(const std::filesystem::path &file)
        {
            slipAt.store(-1);
            markPath[0] = 0;
            const std::wstring wide = file.wstring();
            if (wide.empty() || wide.size() >= std::size(markPath)) return;
            std::copy(wide.begin(), wide.end(), markPath);
            markPath[wide.size()] = 0;
        }

        void Publish(const std::string &session, const std::string &written)
        {
            if (!markPath[0]) return;
            const std::string body = ledger::Mark(session, ledger::Seal(written));
            if (body.empty() || body.size() > sizeof(Slip::body)) return;
            const int next = slipAt.load() == 0 ? 1 : 0;
            std::memcpy(slips[next].body, body.data(), body.size());
            slips[next].size = static_cast<DWORD>(body.size());
            slipAt.store(next);
        }

        struct Memo
        {
            bool ok = false;
            unsigned long pid = 0;
            std::string session;
            bool clean = false;
            std::string last;
        };

        void Named(const wchar_t *tag)
        {
            reloadName[0] = 0;
            std::wstring name = L"ZSDK_RELOAD_";
            for (const wchar_t *p = tag; p && *p; ++p)
            {
                const wchar_t c = *p;
                if ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z')) name += c;
                else if (c >= L'a' && c <= L'z') name += static_cast<wchar_t>(c - 32);
                else name += L'_';
            }
            if (name.size() >= std::size(reloadName)) return;
            std::copy(name.begin(), name.end(), reloadName);
            reloadName[name.size()] = 0;
        }

        Memo Recall()
        {
            Memo out;
            if (!reloadName[0]) return out;
            wchar_t value[192]{};
            const DWORD n = GetEnvironmentVariableW(reloadName, value, static_cast<DWORD>(std::size(value)));
            if (!n || n >= std::size(value)) return out;
            std::string text;
            for (DWORD i = 0; i < n; ++i)
            {
                if (value[i] < 0x20 || value[i] > 0x7E) return out;
                text += static_cast<char>(value[i]);
            }
            std::vector<std::string> parts;
            size_t from = 0;
            while (from <= text.size())
            {
                const size_t to = text.find(' ', from);
                parts.push_back(text.substr(from, to == std::string::npos ? std::string::npos : to - from));
                if (to == std::string::npos) break;
                from = to + 1;
            }
            if (parts.size() != 4 || parts[0].empty() || parts[0].size() > 10) return out;
            for (const char c : parts[0])
                if (c < '0' || c > '9') return out;
            if (!ledger::Session(parts[1]) || (parts[2] != "0" && parts[2] != "1")) return out;
            if (parts[3] != "-" && !ledger::Session(parts[3])) return out;
            out.pid = std::strtoul(parts[0].c_str(), nullptr, 10);
            out.session = parts[1];
            out.clean = parts[2] == "1";
            out.last = parts[3] == "-" ? std::string() : parts[3];
            out.ok = true;
            return out;
        }

        void Forget()
        {
            if (reloadName[0]) SetEnvironmentVariableW(reloadName, nullptr);
        }

        bool WriteNow(const char *why)
        {
            if (frozen.load()) return false;
            std::lock_guard<std::mutex> once(fileGate);
            std::filesystem::path base;
            std::string body;
            std::string session;
            {
                std::lock_guard<std::mutex> hold(gate);
                if (folder.empty()) return false;
                base = folder;
                body = TextRaw(Held().data, kept);
                session = kept.session;
            }
            const bool ok = WriteAtomic(base / app::Get().identity.ini, body);
            if (ok) Publish(session, body);
            else Log("safety: the ini write failed (%s)", why);
            return ok;
        }

        bool SaveSettings()
        {
            if (frozen.load()) return false;
            std::lock_guard<std::mutex> once(fileGate);
            std::filesystem::path base;
            std::string body;
            std::string session;
            {
                std::lock_guard<std::mutex> hold(gate);
                if (!dirty || folder.empty()) return true;
                body = TextRaw(Held().data, kept);
                session = kept.session;
                base = folder;
                dirty = false;
            }
            const bool ok = WriteAtomic(base / app::Get().identity.ini, body);
            if (ok) Publish(session, body);
            uint64_t delay = kSaveDelayMs;
            {
                std::lock_guard<std::mutex> hold(gate);
                if (ok) saveDelay = kSaveDelayMs;
                else
                {
                    dirty = true;
                    changedAt = GetTickCount64();
                    saveDelay = std::min(saveDelay * 2, kSaveBackoffMs);
                }
                delay = saveDelay;
            }
            if (ok) Note(1, "settings saved");
            else Rare("settings save", "settings save failed, retrying in %llu s", static_cast<unsigned long long>(delay / 1000));
            return ok;
        }

        int LevelOf(const void *data)
        {
            const Schema &schema = Need();
            return schema.level ? schema.level(data) : 1;
        }
    }

    const Schema &Bound()
    {
        return Need();
    }

    void Check(void *(*make)())
    {
        if (Need().make != make) throw std::logic_error("the settings type differs from the bound schema");
    }

    std::string Real(double value)
    {
        if (value == 0.0 || !std::isfinite(value)) value = 0.0;
        char buffer[32]{};
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%.4f", value);
        std::string out = buffer;
        while (!out.empty() && out.back() == '0') out.pop_back();
        if (!out.empty() && out.back() == '.') out.pop_back();
        if (out == "-0") out = "0";
        return out;
    }

    std::string Stamp()
    {
        SYSTEMTIME t{};
        GetLocalTime(&t);
        char text[32]{};
        _snprintf_s(text, sizeof(text), _TRUNCATE, "%04u-%02u-%02u %02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
        return text;
    }

    void SanitizeRaw(void *data)
    {
        const Schema &schema = Need();
        Clean clean;
        schema.visit(data, clean);
        if (schema.fix) schema.fix(data);
    }

    void ParseRaw(std::string_view text, void *data, Kept *keep)
    {
        const Schema &schema = Need();
        size_t from = 0;
        if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
            from = 3;
        std::string section = schema.head ? schema.head : "general";
        while (from < text.size())
        {
            size_t end = text.find('\n', from);
            if (end == std::string_view::npos) end = text.size();
            const std::string line = Trim(text.substr(from, end - from));
            from = end + 1;
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line[0] == '[')
            {
                const size_t close = line.find(']');
                section = Lower(Trim(std::string_view(line).substr(1, close == std::string::npos ? std::string::npos : close - 1)));
                continue;
            }
            const size_t equal = line.find('=');
            if (equal == std::string::npos) continue;
            const std::string key = Trim(std::string_view(line).substr(0, equal));
            const std::string value = Trim(std::string_view(line).substr(equal + 1));
            if (key.empty()) continue;
            if (section == "safety")
            {
                if (!keep) continue;
                const std::string k = Lower(key);
                if (k == "session") keep->session = value;
                else if (k == "pending") keep->pending = value;
                else if (k == "disabled") keep->disabled = List(value);
                else if (k == "watch") keep->watch = List(value);
                else if (k == "addon") keep->addon = value;
                else if (keep->unknown.size() < Limits::kUnknownKept) keep->unknown.push_back(Entry{ section, key, value });
                continue;
            }
            if (const Owned *owned = OwnedOf(schema, section))
            {
                if (!keep) continue;
                auto &map = keep->owned[section];
                if (map.size() < owned->cap || map.count(key)) map[key] = value;
                continue;
            }
            if (section == "proven")
            {
                if (keep) keep->proven[key] = value;
                continue;
            }
            const std::string k = Lower(key);
            Assign assign(section, k, value);
            schema.visit(data, assign);
            if (assign.known) continue;
            if (keep && keep->unknown.size() < Limits::kUnknownKept) keep->unknown.push_back(Entry{ section, key, value });
        }
        if (keep && keep->disabled.size() > Limits::kDisabledKept) keep->disabled.erase(keep->disabled.begin(), keep->disabled.end() - static_cast<std::ptrdiff_t>(Limits::kDisabledKept));
        if (keep && keep->watch.size() > Limits::kWatchCap) keep->watch.resize(Limits::kWatchCap);
    }

    std::string TextRaw(const void *data, const Kept &keep)
    {
        const Schema &schema = Need();
        Box copy(schema);
        schema.copy(copy.data, data);
        std::string out;
        Writer writer(out, keep);
        schema.visit(copy.data, writer);
        writer.Finish();
        writer.Head("safety");
        writer.Line("session", keep.session);
        writer.Line("pending", keep.pending);
        writer.Line("disabled", Joined(keep.disabled));
        writer.Line("watch", Joined(keep.watch));
        writer.Rest("safety");
        writer.Line("addon", keep.addon);
        writer.Head("proven");
        for (const auto &[key, value] : keep.proven) writer.Line(key, value);
        for (size_t i = 0; i < schema.ownedCount; ++i)
        {
            writer.Head(schema.owned[i].name);
            const auto found = keep.owned.find(schema.owned[i].name);
            if (found == keep.owned.end()) continue;
            for (const auto &[key, value] : found->second) writer.Line(key, value);
        }
        std::vector<std::string> others;
        for (const Entry &e : keep.unknown)
            if (!KnownSection(schema, copy.data, e.section) && e.section != "safety" && std::find(others.begin(), others.end(), e.section) == others.end()) others.push_back(e.section);
        for (const std::string &name : others)
        {
            writer.Head(name);
            writer.Rest(name);
        }
        return out;
    }

    bool WriteAtomic(const std::filesystem::path &path, std::string_view body)
    {
        std::error_code code;
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), code);
        std::filesystem::path temp = path;
        temp += L".tmp";
        const HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        bool ok = true;
        size_t done = 0;
        while (ok && done < body.size())
        {
            const DWORD chunk = static_cast<DWORD>(std::min(body.size() - done, static_cast<size_t>(1u << 20)));
            DWORD wrote = 0;
            ok = ::WriteFile(file, body.data() + done, chunk, &wrote, nullptr) && wrote == chunk;
            done += wrote;
        }
        if (ok) ok = FlushFileBuffers(file) != 0;
        CloseHandle(file);
        if (ok) ok = MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok) DeleteFileW(temp.c_str());
        return ok;
    }

    bool ReadFile(const std::filesystem::path &path, std::string &out, size_t limit)
    {
        std::error_code code;
        const auto size = std::filesystem::file_size(path, code);
        if (code || size > limit) return false;
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        out.assign(static_cast<size_t>(size), '\0');
        file.read(out.data(), static_cast<std::streamsize>(size));
        return static_cast<size_t>(file.gcount()) == size;
    }

    bool Open(const std::filesystem::path &where)
    {
        const Identity &id = app::Get().identity;
        const Schema &schema = Need();
        frozen.store(false);
        std::filesystem::path base = where;
        if (base.empty())
        {
            wchar_t local[MAX_PATH]{};
            const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
            if (!n || n >= MAX_PATH) return false;
            base = std::filesystem::path(local) / id.folder;
        }
        std::error_code code;
        std::filesystem::create_directories(base, code);
        if (code) return false;
        const std::filesystem::path log = base / id.log;
        {
            std::error_code sized;
            if (std::filesystem::file_size(log, sized) > kLogStart && !sized) log::Roll(log, base / id.rolled);
            log::Open(log, base / id.rolled);
        }
        Box raw(schema);
        Kept keep;
        std::string text;
        const bool had = ReadFile(base / id.ini, text);
        if (had) ParseRaw(text, raw.data, &keep);
        SanitizeRaw(raw.data);
        const std::filesystem::path stop = ExitFile(base);
        std::string mark;
        const bool marked = ReadFile(stop, mark, ledger::kMarkMost);
        DWORD error = 0;
        const bool removed = Drop(stop, error);
        Named(id.folder);
        const Memo memo = Recall();
        const bool reloaded = had && memo.ok && memo.pid == GetCurrentProcessId() && memo.session == keep.session;
        const std::string seal = had ? ledger::Seal(text) : std::string();
        const std::string vouched = reloaded ? ledger::Mark(keep.session, seal) : std::string();
        ledger::Leftover left = ledger::Judge(reloaded ? std::string_view(vouched) : marked ? std::string_view(mark) : std::string_view(), keep.session, seal, keep.pending, keep.watch);
        if (reloaded) Forget();
        const std::string last = ledger::Session(keep.session) ? keep.session : std::string();
        const std::string shown = last.empty() ? std::string("-") : last;
        const std::string pending = keep.pending;
        const std::vector<std::string> watch = keep.watch;
        const std::vector<std::string> before = keep.disabled;
        std::vector<std::string> fresh;
        std::string dropped;
        Fell(keep.disabled, left.fallen, fresh, dropped);
        keep.pending.clear();
        keep.watch.clear();
        keep.session = Fresh();
        std::vector<std::string> unbuilt;
        for (auto it = keep.proven.begin(); it != keep.proven.end();)
        {
            if (Built(it->second))
            {
                ++it;
                continue;
            }
            unbuilt.push_back(it->first);
            it = keep.proven.erase(it);
        }
        const std::string self = ledger::Addon();
        const std::string former = keep.addon;
        const size_t renewed = former != self ? keep.proven.size() : 0;
        if (former != self) keep.proven.clear();
        keep.addon = self;
        std::string body;
        {
            std::lock_guard<std::mutex> hold(gate);
            folder = base;
            schema.copy(Held().data, raw.data);
            kept = keep;
            booted = fresh;
            closed = left.closed;
            previous = reloaded ? memo.last : last;
            inflight.clear();
            dirty = false;
            body = TextRaw(Held().data, kept);
        }
        log::SetLevel(LevelOf(raw.data));
        revision.fetch_add(1);
        bool wrote = false;
        {
            std::lock_guard<std::mutex> once(fileGate);
            Aim(stop);
            wrote = WriteAtomic(base / id.ini, body);
            if (wrote) Publish(keep.session, body);
        }
        const bool doubt = left.clean && !removed && !wrote;
        if (doubt)
        {
            left = ledger::Judge({}, last, {}, pending, watch);
            keep.disabled = before;
            fresh.clear();
            dropped.clear();
            Fell(keep.disabled, left.fallen, fresh, dropped);
            {
                std::lock_guard<std::mutex> hold(gate);
                kept.disabled = keep.disabled;
                booted = fresh;
                closed.clear();
            }
            revision.fetch_add(1);
        }
        cleanStart = reloaded ? memo.clean : left.clean;
        stuck = !removed;
        Log("%s %s data at %s, settings %s%s", id.name, id.version, log::Shown(base).c_str(), had ? "loaded" : "created", wrote ? "" : ", the ini write failed");
        const uint32_t windows = app::Get().policy.watchKept;
        if (windows == 0 || windows > Limits::kWatchCap)
            Log("safety: policy watchKept %u is refused, it must be 1 to %zu, the ledger keeps %zu windows", static_cast<unsigned>(windows), Limits::kWatchCap, Limits::kWatchKept);
        if (reloaded && !doubt)
            Log("safety: loaded again in the same game: the load before (run %s) was unloaded, not crashed%s%s; the game before it %s (run %s)", shown.c_str(),
                left.closed.empty() ? "" : ", the crash windows it left open are closed: ", Joined(left.closed).c_str(), memo.clean ? "ended cleanly" : "did not end cleanly",
                memo.last.empty() ? "-" : memo.last.c_str());
        else if (left.clean)
            Log("safety: the last run %s ended cleanly (exit mark found)%s%s", shown.c_str(), left.closed.empty() ? ", no crash window was left open" : ", the crash windows it left open are closed: ",
                Joined(left.closed).c_str());
        else if (doubt) Log("safety: the exit mark of the last run %s can be neither removed nor outdated by a new ini, it is not trusted and the run counts as a crash", shown.c_str());
        else if (left.older)
            Log("safety: the last run is not judged, written by an older ZoneSDK (no session= line), its crash markers are cleared once and nothing is disabled%s%s", left.cleared.empty() ? "" : ": ",
                Joined(left.cleared).c_str());
        else if (marked) Log("safety: the exit mark does not match the last run %s, it counts as a crash", shown.c_str());
        else if (!left.fallen.empty()) Log("safety: no clean exit mark from the last run %s", shown.c_str());
        if (!removed) Log("safety: the old exit mark could not be removed (error %lu), %s", static_cast<unsigned long>(error), wrote ? "it names an old run and is ignored, the next clean exit may not be marked" : "nothing on disk could change");
        for (const std::string &key : fresh) Log("safety: %s pending or watched when the last run ended, disabled", key.c_str());
        if (!dropped.empty()) Log("safety: the disabled list is full, the oldest keys are enabled again: %s", dropped.c_str());
        if (!unbuilt.empty()) Log("safety: proofs without a known game build are dropped: %s", Joined(unbuilt).c_str());
        if (renewed)
            Log("safety: the add-on build changed from %s to %s, %zu proofs are dropped and first calls carry their crash markers again", former.empty() ? "an unrecorded build" : former.c_str(), self.c_str(),
                renewed);
        if (!keep.disabled.empty()) Log("safety: disabled after a crash: %s", Joined(keep.disabled).c_str());
        Flush();
        return true;
    }

    void Close()
    {
        Save();
        Flush();
    }

    std::filesystem::path Folder()
    {
        std::lock_guard<std::mutex> hold(gate);
        return folder;
    }

    std::filesystem::path LogPath()
    {
        return log::Path();
    }

    std::filesystem::path ExitPath()
    {
        std::lock_guard<std::mutex> hold(gate);
        return folder.empty() ? std::filesystem::path() : ExitFile(folder);
    }

    void ReadRaw(void *out)
    {
        std::lock_guard<std::mutex> hold(gate);
        Need().copy(out, Held().data);
    }

    std::string Language()
    {
        std::lock_guard<std::mutex> hold(gate);
        const Schema &schema = Need();
        const char *code = schema.language ? schema.language(Held().data) : nullptr;
        return code && lang::Valid(code) ? std::string(code) : std::string("auto");
    }

    uint64_t Revision()
    {
        return revision.load();
    }

    void Touch()
    {
        revision.fetch_add(1);
    }

    void ApplyRaw(const void *wanted)
    {
        const Schema &schema = Need();
        Box clean(schema);
        schema.copy(clean.data, wanted);
        SanitizeRaw(clean.data);
        {
            std::lock_guard<std::mutex> hold(gate);
            if (schema.same(clean.data, Held().data)) return;
            schema.copy(Held().data, clean.data);
            dirty = true;
            changedAt = GetTickCount64();
        }
        log::SetLevel(LevelOf(clean.data));
        revision.fetch_add(1);
    }

    bool Dirty()
    {
        std::lock_guard<std::mutex> hold(gate);
        return dirty;
    }

    Kept Ledger()
    {
        std::lock_guard<std::mutex> hold(gate);
        return kept;
    }

    bool Save()
    {
        return SaveSettings();
    }

    void Tick()
    {
        const uint64_t now = GetTickCount64();
        if (log::Due(now))
        {
            log::Hushed(now);
            Flush();
        }
        bool due = false;
        {
            std::lock_guard<std::mutex> hold(gate);
            due = dirty && Age(now, changedAt) >= saveDelay;
        }
        if (due) SaveSettings();
    }

    std::map<std::string, std::string> Section(std::string_view name)
    {
        std::lock_guard<std::mutex> hold(gate);
        const auto found = kept.owned.find(std::string(name));
        return found == kept.owned.end() ? std::map<std::string, std::string>{} : found->second;
    }

    bool Store(std::string_view name, const std::map<std::string, std::string> &values)
    {
        if (frozen.load()) return false;
        {
            std::lock_guard<std::mutex> hold(gate);
            kept.owned[std::string(name)] = values;
        }
        revision.fetch_add(1);
        return WriteNow(std::string(name).c_str());
    }

    bool Put(std::string_view name, const std::string &key, const std::string &value)
    {
        if (key.empty() || frozen.load()) return false;
        const Owned *owned = OwnedOf(Need(), std::string(name));
        const size_t cap = owned ? owned->cap : 16;
        {
            std::lock_guard<std::mutex> hold(gate);
            auto &map = kept.owned[std::string(name)];
            const auto found = map.find(key);
            if (found != map.end() && found->second == value) return true;
            if (found == map.end() && map.size() >= cap) return false;
            map[key] = value;
            dirty = true;
            changedAt = GetTickCount64();
        }
        return true;
    }
}


namespace zsdk::ledger
{
    namespace
    {
        struct Preload
        {
            const char *build;
            const char *key;
        };

        constexpr Preload kPreloaded[] = {
#include "zsdk_proven.inc"
            { "", "" },
        };
    }

    bool Preloaded(std::string_view key, std::string_view build)
    {
        for (const Preload &one : kPreloaded)
            if (one.key[0] && build == one.build && key == one.key) return true;
        return false;
    }

    bool Preloaded(std::string_view key)
    {
        return Preloaded(key, exe::Short());
    }

    size_t PreloadCount()
    {
        size_t n = 0;
        for (const Preload &one : kPreloaded)
            if (one.key[0]) ++n;
        return n;
    }

    bool Session(std::string_view id)
    {
        if (id.empty() || id.size() > kSessionMost) return false;
        for (const char c : id)
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
        return true;
    }

    std::string Seal(std::string_view body)
    {
        unsigned long long hash = 1469598103934665603ull;
        for (const char c : body)
        {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ull;
        }
        char out[24]{};
        _snprintf_s(out, sizeof(out), _TRUNCATE, "%016llX", hash);
        return out;
    }

    std::string Mark(std::string_view session, std::string_view seal)
    {
        if (!Session(session) || !Session(seal)) return {};
        std::string out = "clean ";
        out += session;
        out += ' ';
        out += seal;
        out += "\r\n";
        return out.size() <= kMarkMost ? out : std::string();
    }

    bool Marked(std::string_view mark, std::string_view session, std::string_view seal)
    {
        if (mark.size() > kMarkMost) return false;
        while (!mark.empty() && (mark.back() == '\r' || mark.back() == '\n' || mark.back() == ' ' || mark.back() == '\t' || mark.back() == '\0')) mark.remove_suffix(1);
        const std::string want = Mark(session, seal);
        if (want.size() < 2) return false;
        return mark == std::string_view(want).substr(0, want.size() - 2);
    }

    Leftover Judge(std::string_view mark, std::string_view session, std::string_view seal, std::string_view pending, const std::vector<std::string> &watch)
    {
        Leftover out;
        out.older = session.empty() && !seal.empty();
        out.clean = !out.older && !seal.empty() && Marked(mark, session, seal);
        const auto add = [](std::vector<std::string> &list, std::string_view key) {
            if (key.empty() || std::find(list.begin(), list.end(), key) != list.end()) return;
            list.emplace_back(key);
        };
        if (out.older)
        {
            add(out.cleared, pending);
            for (const std::string &key : watch) add(out.cleared, key);
            return out;
        }
        add(out.fallen, pending);
        for (const std::string &key : watch)
        {
            if (!out.clean) add(out.fallen, key);
            else if (key != pending) add(out.closed, key);
        }
        return out;
    }

    bool Ended(bool gameThread, int runtimes, bool dropped)
    {
        return gameThread && dropped && runtimes <= 0;
    }

    bool CleanStart()
    {
        return ini::cleanStart.load();
    }

    bool Stuck()
    {
        return ini::stuck.load();
    }

    std::vector<std::string> Closed()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::closed;
    }

    std::string Current()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::kept.session;
    }

    std::string Last()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::previous;
    }

    namespace raw
    {
        bool Arm(const std::string &key)
        {
            if (key.empty() || ini::frozen.load()) return false;
            {
                std::lock_guard<std::mutex> hold(ini::gate);
                ini::kept.pending = key;
                ini::inflight = key;
            }
            Log("safety: first call %s", key.c_str());
            Flush();
            return ini::WriteNow("arm");
        }

        bool Prove(const std::string &key, const std::string &build)
        {
            if (key.empty() || ini::frozen.load()) return false;
            const bool known = ini::Built(build);
            {
                std::lock_guard<std::mutex> hold(ini::gate);
                if (ini::kept.pending == key) ini::kept.pending.clear();
                if (ini::inflight == key) ini::inflight.clear();
                if (known) ini::kept.proven[key] = build;
            }
            if (known) Log("safety: %s proven", key.c_str());
            else Log("safety: %s returned, no proof is kept without a known game build", key.c_str());
            return ini::WriteNow("prove");
        }

        bool Settle()
        {
            if (ini::frozen.load()) return false;
            {
                std::lock_guard<std::mutex> hold(ini::gate);
                ini::inflight.clear();
                if (ini::kept.pending.empty()) return true;
                ini::kept.pending.clear();
            }
            return ini::WriteNow("settle");
        }

        bool Disable(const std::string &key)
        {
            if (key.empty() || ini::frozen.load()) return false;
            std::string dropped;
            {
                std::lock_guard<std::mutex> hold(ini::gate);
                if (ini::kept.pending == key) ini::kept.pending.clear();
                if (ini::inflight == key) ini::inflight.clear();
                dropped = ini::Keep(ini::kept.disabled, key);
            }
            ini::revision.fetch_add(1);
            Log("safety: %s disabled after a fault", key.c_str());
            if (!dropped.empty()) Log("safety: the disabled list is full, the oldest keys are enabled again: %s", dropped.c_str());
            return ini::WriteNow("disable");
        }

        void Freeze()
        {
            ini::frozen.store(true);
        }

        void Leave()
        {
            ini::frozen.store(true);
            const int at = ini::slipAt.exchange(-1);
            if (at < 0 || at > 1 || !ini::markPath[0]) return;
            const ini::Slip &slip = ini::slips[at];
            if (!slip.size || slip.size > sizeof(slip.body)) return;
            const HANDLE file = ::CreateFileW(ini::markPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) return;
            DWORD wrote = 0;
            ::WriteFile(file, slip.body, slip.size, &wrote, nullptr);
            CloseHandle(file);
        }

        void Unload()
        {
            if (ini::frozen.load() || !ini::reloadName[0]) return;
            std::string session;
            std::string last;
            {
                std::lock_guard<std::mutex> hold(ini::gate);
                session = ini::kept.session;
                last = ini::previous;
            }
            if (!Session(session) || (!last.empty() && !Session(last))) return;
            const std::string value = std::to_string(GetCurrentProcessId()) + " " + session + (ini::cleanStart.load() ? " 1 " : " 0 ") + (last.empty() ? std::string("-") : last);
            SetEnvironmentVariableW(ini::reloadName, std::wstring(value.begin(), value.end()).c_str());
        }

        std::wstring ReloadName()
        {
            return ini::reloadName;
        }
    }

    std::string Proven(std::string_view key)
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        const auto found = ini::kept.proven.find(std::string(key));
        return found == ini::kept.proven.end() ? std::string() : found->second;
    }

    std::vector<std::string> Disabled()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::kept.disabled;
    }

    bool Blocked(std::string_view key)
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return std::find(ini::kept.disabled.begin(), ini::kept.disabled.end(), key) != ini::kept.disabled.end();
    }

    bool Full()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::kept.disabled.size() >= ini::Limits::kDisabledKept;
    }

    static_assert(Policy{}.watchKept == ini::Limits::kWatchKept && ini::Limits::kWatchKept <= ini::Limits::kWatchCap);

    size_t WatchLimit()
    {
        const uint32_t wanted = app::Get().policy.watchKept;
        return wanted >= 1 && wanted <= ini::Limits::kWatchCap ? static_cast<size_t>(wanted) : ini::Limits::kWatchKept;
    }

    std::string Addon()
    {
        HMODULE image = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&WatchLimit), &image);
        unsigned long stamp = 0;
        if (image)
        {
            const auto *base = reinterpret_cast<const unsigned char *>(image);
            const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
            const auto *nt = dos->e_magic == IMAGE_DOS_SIGNATURE ? reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew) : nullptr;
            if (nt && nt->Signature == IMAGE_NT_SIGNATURE) stamp = nt->FileHeader.TimeDateStamp;
        }
        const char *version = app::Get().identity.version;
        char out[96]{};
        _snprintf_s(out, sizeof(out), _TRUNCATE, "%s/%08lX", version && *version ? version : "?", stamp);
        return out;
    }

    void Retry()
    {
        if (ini::frozen.load()) return;
        std::string was;
        std::string held;
        {
            std::lock_guard<std::mutex> hold(ini::gate);
            was = ini::Joined(ini::kept.disabled);
            ini::kept.disabled.clear();
            if (!ini::kept.pending.empty() && ini::kept.pending == ini::inflight) held = ini::kept.pending;
            else ini::kept.pending.clear();
            ini::booted.clear();
        }
        ini::revision.fetch_add(1);
        Log("safety: try again, cleared %s", was.empty() ? "nothing" : was.c_str());
        if (!held.empty()) Log("safety: %s is running now, its crash marker stays", held.c_str());
        ini::WriteNow("retry");
    }

    std::vector<std::string> Booted()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::booted;
    }

    bool Watch(const std::string &key)
    {
        if (key.empty() || ini::frozen.load()) return false;
        {
            std::lock_guard<std::mutex> hold(ini::gate);
            if (std::find(ini::kept.watch.begin(), ini::kept.watch.end(), key) != ini::kept.watch.end()) return true;
            if (ini::kept.watch.size() >= WatchLimit()) return false;
            ini::kept.watch.push_back(key);
        }
        Log("safety: watching %s", key.c_str());
        Flush();
        return ini::WriteNow("watch");
    }

    bool Unwatch(const std::string &key, const std::string &build)
    {
        if (key.empty() || ini::frozen.load()) return false;
        const bool known = ini::Built(build);
        {
            std::lock_guard<std::mutex> hold(ini::gate);
            const auto found = std::find(ini::kept.watch.begin(), ini::kept.watch.end(), key);
            if (found == ini::kept.watch.end() && !known) return true;
            if (found != ini::kept.watch.end()) ini::kept.watch.erase(found);
            if (known) ini::kept.proven[key] = build;
        }
        Log("safety: %s %s", key.c_str(), known ? "proven" : "no longer watched");
        return ini::WriteNow("unwatch");
    }

    bool Calm()
    {
        if (ini::frozen.load()) return false;
        std::string was;
        {
            std::lock_guard<std::mutex> hold(ini::gate);
            if (ini::kept.watch.empty()) return true;
            was = ini::Joined(ini::kept.watch);
            ini::kept.watch.clear();
        }
        Log("safety: %s no longer watched, the game quits", was.c_str());
        return ini::WriteNow("calm");
    }

    std::vector<std::string> Watched()
    {
        std::lock_guard<std::mutex> hold(ini::gate);
        return ini::kept.watch;
    }

    Window::Window(std::string wanted) : key(std::move(wanted))
    {
        open = true;
        if (key.empty() || ini::frozen.load())
        {
            refused = true;
            return;
        }
        if (Blocked(key))
        {
            refused = true;
            Rare(key.c_str(), "safety: the window %s is disabled after a crash, it stays closed", key.c_str());
            return;
        }
        const std::string build = exe::Short();
        const bool needed = !ini::Built(build) || Proven(key) != build;
        if (!needed) return;
        armed = Watch(key);
        if (!armed)
        {
            refused = true;
            Rare(key.c_str(), "safety: the crash window %s could not be written, it stays closed", key.c_str());
        }
    }

    Window::~Window()
    {
        if (ini::frozen.load()) return;
        if (open && armed) Unwatch(key, std::string());
    }

    void Window::Close()
    {
        if (!open) return;
        open = false;
        if (armed && !ini::frozen.load()) Unwatch(key, exe::Short());
        armed = false;
    }

    bool Window::Armed() const
    {
        return armed;
    }

    bool Window::Open() const
    {
        return open;
    }

    bool Window::Refused() const
    {
        return refused;
    }

    const std::string &Window::Key() const
    {
        return key;
    }
}