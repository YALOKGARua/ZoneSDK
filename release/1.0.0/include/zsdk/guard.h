#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zsdk::guard
{
    struct Build
    {
        const char *id;
        const char *sha256;
        uint32_t imageSize;
        uint32_t stamp;
        uint32_t objectsRoot;
        uint32_t objectsCount;
    };

    inline constexpr Build k61bc1e03{ "61bc1e03", "61BC1E030740CEBC30CF1DAD0C86CF65E39E12FF0500225821D684181E08D56B", 0xB963000u, 0xCD15CBEEu, 0xA0E68D0u, 0xA0E68E4u };
    inline constexpr Build k2bd3f115{ "2bd3f115", "2BD3F115B959A8EEE7E5C4B42B0142DD0025BD437E1014F92DFC60B9AFEE5479", 0xB954000u, 0x2B1AEEBFu, 0xA0D4990u, 0xA0D49A4u };
    inline constexpr const Build *kBuilds[] = { &k61bc1e03, &k2bd3f115 };
    inline constexpr uintptr_t kImageBase = 0x140000000ull;

    struct Guard
    {
        uint32_t rva;
        const char *bytes;
    };

    enum class Need : uint8_t { Bytes, ImageSize, KnownBuild };

    struct Set
    {
        const char *name;
        const Build *build;
        const Guard *guards;
        size_t count;
        Need need;
    };

    enum class State : uint8_t { Pending, Ok, Off };

    struct Report
    {
        int ok = 0;
        int total = 0;
        uint32_t image = 0;
        bool imageOk = false;
        bool known = false;
        std::string bad;
        State state = State::Pending;
    };

    struct Anchor
    {
        uint64_t va;
        const char *bytes;
        int disp;
        int next;
    };

    namespace detail
    {
        consteval int Nibble(char c)
        {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        }

        consteval int ByteAt(const char *bytes, int index)
        {
            int seen = 0;
            for (int i = 0; bytes[i];)
            {
                if (bytes[i] == ' ')
                {
                    ++i;
                    continue;
                }
                if (!bytes[i + 1]) return -2;
                if (seen == index)
                {
                    if (bytes[i] == '?' && bytes[i + 1] == '?') return -1;
                    const int hi = Nibble(bytes[i]);
                    const int lo = Nibble(bytes[i + 1]);
                    return hi < 0 || lo < 0 ? -2 : hi * 16 + lo;
                }
                ++seen;
                i += 2;
            }
            return -2;
        }

        consteval bool Holds(const char *bytes, uint32_t value, int offset)
        {
            const int width = value < 0x80u ? 1 : 4;
            for (int k = 0; k < width; ++k)
                if (ByteAt(bytes, offset + k) != static_cast<int>((value >> (8 * k)) & 0xFFu)) return false;
            return true;
        }

        consteval bool Rip(uint32_t rva, const char *bytes, int disp, int next, uint32_t target, int addend)
        {
            uint32_t raw = 0;
            for (int k = 0; k < 4; ++k)
            {
                const int b = ByteAt(bytes, disp + k);
                if (b < 0) return false;
                raw |= static_cast<uint32_t>(b) << (8 * k);
            }
            const int64_t signedDisp = static_cast<int32_t>(raw);
            return static_cast<int64_t>(rva) + next + signedDisp == static_cast<int64_t>(target) + addend;
        }

        consteval bool Named(const char *text)
        {
            return text && text[0];
        }
    }

    enum class Proof : uint8_t { Guard, Rip, Reflected, Unproven };

    struct Field
    {
        const char *name;
        uint32_t at;
        const Set *set;
        int guard;
        int offset;
        const char *reflectedClass;
        const char *reflectedProperty;
        Proof proof;

        consteval Field(const char *n, uint32_t a, const Set &s, int g, int o)
            : name(n), at(a), set(&s), guard(g), offset(o), reflectedClass(nullptr), reflectedProperty(nullptr), proof(Proof::Guard)
        {
            if (!detail::Named(n) || g < 0 || static_cast<size_t>(g) >= s.count || !detail::Holds(s.guards[g].bytes, a, o)) throw "field offset is not in its guard bytes";
        }

        consteval Field(const char *n, uint32_t a, const char *klass, const char *property)
            : name(n), at(a), set(nullptr), guard(-1), offset(-1), reflectedClass(klass), reflectedProperty(property), proof(Proof::Reflected)
        {
            if (!detail::Named(n) || !detail::Named(klass) || !detail::Named(property)) throw "a reflected field needs a class and a property";
        }

        static consteval Field Rip(const char *n, uint32_t rva, const Set &s, int g, int disp, int next, int addend = 0)
        {
            if (!detail::Named(n) || g < 0 || static_cast<size_t>(g) >= s.count || !detail::Rip(s.guards[g].rva, s.guards[g].bytes, disp, next, rva, addend))
                throw "the RIP target of the guard is not this address";
            Field f(n, rva, "-", "-");
            f.reflectedClass = nullptr;
            f.reflectedProperty = nullptr;
            f.set = &s;
            f.guard = g;
            f.offset = disp;
            f.proof = Proof::Rip;
            return f;
        }

        static consteval Field Unproven(const char *n, uint32_t a)
        {
            if (!detail::Named(n)) throw "an unproven field needs a name";
            Field f(n, a, "-", "-");
            f.reflectedClass = nullptr;
            f.reflectedProperty = nullptr;
            f.proof = Proof::Unproven;
            return f;
        }

        constexpr uintptr_t Raw() const
        {
            return at;
        }
    };

    bool Hex(const char *bytes, std::vector<int> &out);
    bool Matches(uintptr_t at, const char *bytes);
    uintptr_t Base();
    uintptr_t At(const Anchor &anchor);
    uintptr_t Target(uintptr_t at, int disp, int next);
    uintptr_t Resolve(const Anchor &anchor);
    Report Check(const Set &set);
    State Of(const Set &set);
    bool Ready(const Set &set);
    int Recheck();
    bool Usable(const Field &field);
    bool Confirm(const Field &field);
    bool Use(const Field &field, uintptr_t &at);
    const Build *Find(std::string_view sha);
    std::span<const Set *const> Own();
}
