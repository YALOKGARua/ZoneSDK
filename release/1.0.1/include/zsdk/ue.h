#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zsdk::ue
{
    inline constexpr int32_t kIndexAt = 0x0C;
    inline constexpr int32_t kClassAt = 0x10;
    inline constexpr int32_t kNameAt = 0x18;
    inline constexpr int32_t kOuterAt = 0x20;
    inline constexpr int32_t kSuperAt = 0x40;
    inline constexpr uint32_t kGoneStrict = 0x40018030u;
    inline constexpr uint32_t kGoneLegacy = 0x40000030u;
    inline constexpr uint32_t kObjectDoomed = 0x40018000u;
    inline constexpr int32_t kItemFlagsAt = 0x08;
    inline constexpr int32_t kItemClusterAt = 0x0C;
    inline constexpr uint32_t kItemGarbage = 0x00200000u;
    inline constexpr uint32_t kItemUnreachable = 0x10000000u;
    inline constexpr size_t kLendCap = size_t{ 1 } << 20;

    struct Lent
    {
        const wchar_t *data = nullptr;
        int32_t num = 0;
        int32_t max = 0;
    };

    static_assert(sizeof(Lent) == 16, "an FString parameter is a pointer, a length with the zero and a capacity");

    inline Lent Lend(const std::wstring &text)
    {
        if (text.size() >= kLendCap) return Lent{};
        const auto n = static_cast<int32_t>(text.size() + 1);
        return Lent{ text.c_str(), n, n };
    }

    inline Lent Lend(const wchar_t *text)
    {
        if (!text) return Lent{};
        size_t size = 0;
        while (size < kLendCap && text[size]) ++size;
        if (size >= kLendCap) return Lent{};
        const auto n = static_cast<int32_t>(size + 1);
        return Lent{ text, n, n };
    }

    Lent Lend(const std::wstring &&text) = delete;

    struct Param
    {
        std::string name;
        std::string type;
        const void *field = nullptr;
        int32_t at = 0;
        int32_t size = 0;
        uint64_t flags = 0;
    };

    struct Sign
    {
        std::string owner;
        int32_t bytes = 0;
        std::vector<Param> params;
    };

    struct Pick
    {
        std::string name;
        int64_t value = 0;
    };

    struct Mark
    {
        const void *actor = nullptr;
        const void *type = nullptr;
        int32_t index = -1;
        uint64_t name = 0;
        int32_t serial = 0;
    };

    bool Ready();
    bool Bound();
    bool Copy(const void *at, void *into, size_t bytes);

    template<class T> bool Read(uintptr_t at, T &out)
    {
        return at >= 0x10000 && Copy(reinterpret_cast<const void *>(at), &out, sizeof(T));
    }

    bool Held(const void *spot, size_t bytes);
    const void *Grab(const void *thing, int32_t at);
    int32_t Total();
    int32_t Stride();
    const uint8_t *Item(int32_t index);
    const void *At(int32_t index);
    uint32_t Flags(const void *thing);
    const void *ClassOf(const void *thing);
    const void *OuterOf(const void *thing);
    std::string Label(const void *thing);
    std::string Path(const void *thing);
    std::string Word(uint32_t slot);
    bool Kin(const void *type, const void *base);
    bool Inherits(const void *type, const char *name);
    bool Standing(const void *thing, const void *type);
    bool Here(const void *thing, const void *type = nullptr);
    bool Gone(const void *thing);
    bool Doomed(const void *thing);
    std::string Look(const void *thing);
    std::wstring CopyText(const void *at);
    std::string NameText(uint32_t index, uint32_t number);
    int32_t Serial(int32_t index);
    Mark Seal(const void *thing);
    bool Ours(const Mark &mark);
    int32_t PropertyOffset(const void *type, const char *field);
    int32_t Bulk(const void *type);
    const void *FindClass(const char *name);
    const void *FindLive(const char *baseClass);
    const void *Host(const void *type);
    bool Analyze(const void *function, Sign &out);
    const void *Function(const char *owner, const char *name);
    std::vector<const void *> Functions(const char *name);
    bool Choices(const Param &one, std::vector<Pick> &out);
    bool Choose(const std::vector<Pick> &picks, const std::string &word, int64_t &out);
}
