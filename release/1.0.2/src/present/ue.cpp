#include <zsdk/present.h>

#include "../detail/engine_core.h"

namespace zsdk::ue
{
    namespace core = detail::engine;


    bool Ready()
    {
        return core::Ready();
    }

    bool Bound()
    {
        return core::Bound();
    }

    bool Copy(const void *at, void *into, size_t bytes)
    {
        return core::Copy(at, into, bytes);
    }

    bool Held(const void *spot, size_t bytes)
    {
        return core::Held(spot, bytes);
    }

    const void *Grab(const void *thing, int32_t at)
    {
        return core::Grab(thing, at);
    }

    int32_t Total()
    {
        return core::Total();
    }

    int32_t Stride()
    {
        return core::Stride();
    }

    const uint8_t *Item(int32_t index)
    {
        return core::Item(index);
    }

    const void *At(int32_t index)
    {
        return core::At(index);
    }

    uint32_t Flags(const void *thing)
    {
        uint32_t flags = 0;
        if (!thing || !Read(Addr(thing) + 8, flags)) return 0;
        return flags;
    }

    const void *ClassOf(const void *thing)
    {
        return core::ClassOf(thing);
    }

    const void *OuterOf(const void *thing)
    {
        return core::OuterOf(thing);
    }

    std::string Label(const void *thing)
    {
        return core::Label(thing);
    }

    std::string Path(const void *thing)
    {
        return core::Path(thing);
    }

    std::string Word(uint32_t slot)
    {
        return core::Word(slot);
    }

    bool Kin(const void *type, const void *base)
    {
        return core::Kin(type, base);
    }

    bool Inherits(const void *type, const char *name)
    {
        return core::Inherits(type, name);
    }

    bool Standing(const void *thing, const void *type)
    {
        return core::Standing(thing, type);
    }

    bool Here(const void *thing, const void *type)
    {
        if (!thing || Addr(thing) < 0x10000) return false;
        const auto *base = static_cast<const uint8_t *>(thing);
        const void *kind = nullptr;
        int32_t seat = -1;
        if (!Copy(base + kClassAt, &kind, sizeof(kind)) || !kind || (type && kind != type)) return false;
        if (!Copy(base + kIndexAt, &seat, sizeof(seat)) || seat < 0 || seat >= Total()) return false;
        const uint8_t *slot = Item(seat);
        const void *back = nullptr;
        return slot && Copy(slot, &back, sizeof(back)) && back == thing;
    }

    bool Gone(const void *thing)
    {
        uint32_t flags = 0;
        const uint32_t mask = app::Get().policy.gone == GoneFlags::Legacy ? kGoneLegacy : kGoneStrict;
        return !thing || !Read(Addr(thing) + 8, flags) || (flags & mask) != 0;
    }

    namespace
    {
        struct Seen
        {
            bool here = false;
            bool objectRead = false;
            uint32_t object = 0;
            bool itemRead = false;
            uint32_t item = 0;
            int32_t cluster = -1;
        };

        Seen See(const void *thing)
        {
            Seen s;
            if (!thing || Addr(thing) < 0x10000) return s;
            s.objectRead = Read(Addr(thing) + 8, s.object);
            s.here = Here(thing);
            if (!s.here) return s;
            int32_t seat = -1;
            Copy(static_cast<const uint8_t *>(thing) + kIndexAt, &seat, sizeof(seat));
            const uint8_t *slot = Item(seat);
            s.itemRead = slot && Copy(slot + kItemFlagsAt, &s.item, sizeof(s.item));
            if (slot) Copy(slot + kItemClusterAt, &s.cluster, sizeof(s.cluster));
            return s;
        }
    }

    bool Doomed(const void *thing)
    {
        const Seen s = See(thing);
        if (!s.here || !s.objectRead || (s.object & kObjectDoomed) != 0) return true;
        return s.itemRead && (s.item & (kItemGarbage | kItemUnreachable)) != 0;
    }

    std::string Look(const void *thing)
    {
        if (!thing) return "null";
        const Seen s = See(thing);
        const std::string object = s.objectRead ? text::Format("0x%08X", s.object) : std::string("unreadable");
        if (!s.here) return text::Format("%p not in the object array, object flags %s", thing, object.c_str());
        const std::string item = s.itemRead ? text::Format("0x%08X", s.item) : std::string("unreadable");
        return text::Format("%p %s, object flags %s, item flags %s, cluster %d, %s", thing, Label(thing).c_str(), object.c_str(), item.c_str(), s.cluster, Doomed(thing) ? "doomed" : "alive");
    }

    std::wstring CopyText(const void *at)
    {
        Lent view;
        if (!at || Addr(at) < 0x10000 || !Copy(at, &view, sizeof(view))) return {};
        if (!view.data || view.num <= 0 || view.max < view.num || static_cast<size_t>(view.num) > kLendCap) return {};
        std::wstring out(static_cast<size_t>(view.num), L'\0');
        if (!Copy(view.data, out.data(), out.size() * sizeof(wchar_t))) return {};
        const size_t end = out.find(L'\0');
        if (end != std::wstring::npos) out.resize(end);
        return out;
    }

    std::string NameText(uint32_t index, uint32_t number)
    {
        std::string word = Word(index);
        if (word.empty() || number == 0) return word;
        return word + "_" + std::to_string(number - 1);
    }

    int32_t Serial(int32_t index)
    {
        if (index < 0 || Stride() < 0x14) return 0;
        const uint8_t *slot = Item(index);
        int32_t number = 0;
        if (!slot || !Copy(slot + 0x10, &number, sizeof(number))) return 0;
        return number;
    }

    Mark Seal(const void *thing)
    {
        Mark m;
        if (!thing) return m;
        const auto *base = static_cast<const uint8_t *>(thing);
        m.actor = thing;
        m.type = ClassOf(thing);
        Copy(base + kIndexAt, &m.index, sizeof(m.index));
        Copy(base + kNameAt, &m.name, sizeof(m.name));
        m.serial = Serial(m.index);
        return m;
    }

    bool Ours(const Mark &m)
    {
        if (!m.actor || !Here(m.actor, m.type)) return false;
        const auto *base = static_cast<const uint8_t *>(m.actor);
        int32_t index = -1;
        uint64_t name = 0;
        if (!Copy(base + kIndexAt, &index, sizeof(index)) || index != m.index) return false;
        if (!Copy(base + kNameAt, &name, sizeof(name)) || name != m.name) return false;
        return (m.serial == 0 || Serial(index) == m.serial) && !Doomed(m.actor);
    }

    int32_t PropertyOffset(const void *type, const char *field)
    {
        return core::PropertyOffset(type, field);
    }

    int32_t Bulk(const void *type)
    {
        return core::Bulk(type);
    }

    const void *FindClass(const char *name)
    {
        return core::FindClass(name);
    }

    const void *FindLive(const char *baseClass)
    {
        return core::FindLive(baseClass);
    }

    const void *Host(const void *type)
    {
        return core::Host(type);
    }

    bool Analyze(const void *function, Sign &out)
    {
        return core::Analyze(function, out);
    }

    const void *Function(const char *owner, const char *name)
    {
        for (const void *one : core::Exact(name))
            if (Label(OuterOf(one)) == owner) return one;
        return nullptr;
    }

    std::vector<const void *> Functions(const char *name)
    {
        return core::Exact(name);
    }

    bool Choices(const Param &one, std::vector<Pick> &out)
    {
        return core::Choices(one, out);
    }

    bool Choose(const std::vector<Pick> &picks, const std::string &word, int64_t &out)
    {
        for (const Pick &pick : picks)
        {
            const size_t cut = pick.name.rfind("::");
            const std::string bare = cut == std::string::npos ? pick.name : pick.name.substr(cut + 2);
            if (text::Same(word, pick.name) || text::Same(word, bare))
            {
                out = pick.value;
                return true;
            }
        }
        return false;
    }
}
