#pragma once

#include "keys.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zsdk::ini
{
    struct Entry
    {
        std::string section;
        std::string key;
        std::string value;
        friend bool operator==(const Entry &, const Entry &) = default;
    };

    struct Kept
    {
        std::string session;
        std::string pending;
        std::vector<std::string> disabled;
        std::vector<std::string> watch;
        std::map<std::string, std::string> proven;
        std::map<std::string, std::map<std::string, std::string>> owned;
        std::vector<Entry> unknown;
        std::string addon;
        friend bool operator==(const Kept &, const Kept &) = default;
    };

    struct Limits
    {
        static constexpr size_t kUnknownKept = 64;
        static constexpr size_t kDisabledKept = 32;
        static constexpr size_t kWatchKept = 8;
        static constexpr size_t kWatchCap = 32;
        static constexpr size_t kTextMost = 256;
    };

    class Visitor
    {
    public:
        virtual ~Visitor() = default;
        virtual void Flag(const char *section, const char *key, bool &value) = 0;
        virtual void Whole(const char *section, const char *key, int &value, int def, int low, int high) = 0;
        virtual void Real(const char *section, const char *key, double &value, double def, double low, double high) = 0;
        virtual void Word(const char *section, const char *key, std::string &value, const char *def, const char *const *choices, int count) = 0;
        virtual void Pick(const char *section, const char *key, int &value, const char *const *names, int count, int def) = 0;
        virtual void Key(const char *section, const char *key, keys::Chord &value, keys::Chord def) = 0;
        virtual void Text(const char *section, const char *key, std::string &value, size_t most)
        {
            (void)most;
            Word(section, key, value, nullptr, nullptr, 0);
        }
    };

    template<class E, size_t N> void Choice(Visitor &v, const char *section, const char *key, E &value, const char *const (&names)[N], E def)
    {
        int raw = static_cast<int>(value);
        v.Pick(section, key, raw, names, static_cast<int>(N), static_cast<int>(def));
        value = static_cast<E>(raw);
    }

    struct Owned
    {
        const char *name;
        size_t cap;
    };

    struct Schema
    {
        const char *head = "general";
        void (*visit)(void *settings, Visitor &v) = nullptr;
        void (*fix)(void *settings) = nullptr;
        void *(*make)() = nullptr;
        void (*drop)(void *settings) = nullptr;
        void (*copy)(void *to, const void *from) = nullptr;
        bool (*same)(const void *a, const void *b) = nullptr;
        int (*level)(const void *settings) = nullptr;
        const char *(*language)(const void *settings) = nullptr;
        const Owned *owned = nullptr;
        size_t ownedCount = 0;
    };

    template<class S> struct Typed
    {
        static void *Make() { return new S(); }
        static void Drop(void *settings) { delete static_cast<S *>(settings); }
        static void Copy(void *to, const void *from) { *static_cast<S *>(to) = *static_cast<const S *>(from); }
        static bool Same(const void *a, const void *b) { return *static_cast<const S *>(a) == *static_cast<const S *>(b); }
    };

    template<class S, void (*V)(S &, Visitor &)> void VisitThunk(void *settings, Visitor &v)
    {
        V(*static_cast<S *>(settings), v);
    }

    template<class S, void (*F)(S &)> void FixThunk(void *settings)
    {
        F(*static_cast<S *>(settings));
    }

    template<class S, int (*L)(const S &)> int LevelThunk(const void *settings)
    {
        return L(*static_cast<const S *>(settings));
    }

    template<class S, const char *(*G)(const S &)> const char *LanguageThunk(const void *settings)
    {
        return G(*static_cast<const S *>(settings));
    }

    template<class S, void (*V)(S &, Visitor &), void (*F)(S &), int (*L)(const S &), const char *(*G)(const S &)>
    constexpr Schema Make(const char *head, const Owned *owned, size_t ownedCount)
    {
        Schema s;
        s.head = head;
        s.visit = &VisitThunk<S, V>;
        s.fix = &FixThunk<S, F>;
        s.make = &Typed<S>::Make;
        s.drop = &Typed<S>::Drop;
        s.copy = &Typed<S>::Copy;
        s.same = &Typed<S>::Same;
        s.level = &LevelThunk<S, L>;
        s.language = &LanguageThunk<S, G>;
        s.owned = owned;
        s.ownedCount = ownedCount;
        return s;
    }

    const Schema &Bound();
    bool Open(const std::filesystem::path &folder = {});
    void Close();
    std::filesystem::path Folder();
    std::filesystem::path LogPath();
    std::filesystem::path ExitPath();
    void Tick();
    uint64_t Revision();
    void Touch();
    bool Dirty();
    bool Save();
    Kept Ledger();
    std::string Language();

    void ParseRaw(std::string_view text, void *settings, Kept *keep);
    std::string TextRaw(const void *settings, const Kept &keep);
    void SanitizeRaw(void *settings);
    void ReadRaw(void *out);
    void ApplyRaw(const void *settings);
    void Check(void *(*make)());

    template<class S> S Config()
    {
        Check(&Typed<S>::Make);
        S out{};
        ReadRaw(&out);
        return out;
    }

    template<class S> void Apply(const S &settings)
    {
        Check(&Typed<S>::Make);
        ApplyRaw(&settings);
    }

    template<class S> void Parse(std::string_view text, S &settings, Kept *keep = nullptr)
    {
        Check(&Typed<S>::Make);
        ParseRaw(text, &settings, keep);
    }

    template<class S> std::string Text(const S &settings, const Kept &keep = {})
    {
        Check(&Typed<S>::Make);
        return TextRaw(&settings, keep);
    }

    template<class S> S Sanitize(S settings)
    {
        Check(&Typed<S>::Make);
        SanitizeRaw(&settings);
        return settings;
    }

    std::map<std::string, std::string> Section(std::string_view name);
    bool Store(std::string_view name, const std::map<std::string, std::string> &values);
    bool Put(std::string_view name, const std::string &key, const std::string &value);
    std::string Stamp();
    std::string Real(double value);
    bool WriteAtomic(const std::filesystem::path &path, std::string_view body);
    bool ReadFile(const std::filesystem::path &path, std::string &out, size_t limit = 1u << 20);
}
