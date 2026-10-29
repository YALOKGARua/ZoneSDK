#include <zsdk/present.h>

#include "../detail/engine_core.h"
#include "../detail/native_bindings.h"
#include "../detail/trace.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace zsdk::detail
{
    void Trace(const char *shape, ...)
    {
        char buffer[4096]{};
        va_list args;
        va_start(args, shape);
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, shape, args);
        va_end(args);
        Log("engine: %s", buffer);
    }

    void Paint(const char *shape, ...)
    {
        char buffer[2048]{};
        va_list args;
        va_start(args, shape);
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, shape, args);
        va_end(args);
        Log("paint: %s", buffer);
    }
}

namespace zsdk::detail::engine
{
    namespace
    {
        constexpr int32_t kClassAt = 0x10;
        constexpr int32_t kNameAt = 0x18;
        constexpr int32_t kOuterAt = 0x20;
        constexpr int32_t kSuperAt = 0x40;
        constexpr int32_t kNamesAt = 0x40;
        constexpr int32_t kItemFlagsAt = 0x08;
        constexpr uint32_t kItemGone = 0x10200000u;
        constexpr uint32_t kBlockBits = 16;
        constexpr uint32_t kBlockMask = (1u << kBlockBits) - 1u;
        constexpr DWORD kHostRetryMs = 1500;

        using Thunk = void(__fastcall *)(const void *, void *, void *);

        struct Patch
        {
            uintptr_t from;
            uintptr_t upto;
        };

        struct Shape
        {
            int headerAt = 0;
            int charsAt = 2;
            int shift = 6;
        };

        struct Fields
        {
            int32_t listAt = 0x50;
            int32_t nextAt = 0x20;
            int32_t nameAt = 0x28;
            int32_t offsetAt = 0x4C;
        };

        struct Frame
        {
            int32_t thunkAt = -1;
            int32_t localsAt = 0x28;
            int32_t chainAt = -1;
        };

        uint8_t **blocks = nullptr;
        uint8_t *table = nullptr;
        uint8_t *objectCount = nullptr;
        bool poolsReady = false;
        int32_t stride = 24;
        Shape shape;
        Fields fields;
        Frame rig;
        std::vector<int32_t> reins;
        int32_t facetsAt = -1;

        std::atomic<bool> bound{ false };
        std::atomic<bool> proven{ false };
        std::atomic<bool> dusk{ false };
        std::atomic<bool> failed{ false };
        std::atomic<bool> booked{ false };
        const char *snag = "";

        std::vector<Patch> live;
        std::shared_mutex plan;
        std::atomic<uint32_t> era{ 1 };
        std::recursive_mutex work;

        constexpr DWORD kRefreshMs = 3000;
        constexpr DWORD kStaleMs = 1500;

        std::atomic<bool> bookWanted{ false };
        std::shared_mutex bookGate;
        std::unordered_multimap<std::string, const void *> book;
        std::mutex absentGate;
        std::unordered_set<std::string> absent;
        std::atomic<DWORD> swept{ 0 };
        std::atomic<bool> doubt{ false };
        std::atomic<uint32_t> bookEpoch{ 0 };

        void Doubt(const std::string &name)
        {
            if (swept.load() == 0) return;
            std::lock_guard<std::mutex> hold(absentGate);
            if (absent.size() < 4096 && absent.insert(name).second) doubt = true;
        }

        struct Desk
        {
            const void *host = nullptr;
            DWORD tried = 0;
        };

        std::mutex stallGate;
        std::unordered_map<const void *, Desk> stall;

        bool CopyRaw(const void *at, void *into, size_t bytes)
        {
            __try
            {
                memcpy(into, at, bytes);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        void Walk(std::vector<Patch> &into, const std::vector<void *> &skip)
        {
            uint8_t *cursor = nullptr;
            MEMORY_BASIC_INFORMATION info{};
            const DWORD mask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            while (VirtualQuery(cursor, &info, sizeof(info)) == sizeof(info))
            {
                const bool plain = info.State == MEM_COMMIT && (info.Protect & mask) != 0 && (info.Protect & PAGE_GUARD) == 0;
                const bool shunned = std::find(skip.begin(), skip.end(), info.AllocationBase) != skip.end();
                if (plain && !shunned)
                {
                    const uintptr_t from = reinterpret_cast<uintptr_t>(info.BaseAddress);
                    if (!into.empty() && into.back().upto == from) into.back().upto = from + info.RegionSize;
                    else into.push_back({ from, from + info.RegionSize });
                }
                uint8_t *step = static_cast<uint8_t *>(info.BaseAddress) + info.RegionSize;
                if (step <= cursor || reinterpret_cast<uintptr_t>(step) >= 0x7FFFFFFE0000ull) break;
                cursor = step;
            }
        }

        std::vector<void *> Stacks()
        {
            std::vector<void *> found;
            uint8_t *cursor = nullptr;
            MEMORY_BASIC_INFORMATION info{};
            while (VirtualQuery(cursor, &info, sizeof(info)) == sizeof(info))
            {
                if ((info.Protect & PAGE_GUARD) != 0 && info.AllocationBase != nullptr && std::find(found.begin(), found.end(), info.AllocationBase) == found.end())
                    found.push_back(info.AllocationBase);
                uint8_t *step = static_cast<uint8_t *>(info.BaseAddress) + info.RegionSize;
                if (step <= cursor || reinterpret_cast<uintptr_t>(step) >= 0x7FFFFFFE0000ull) break;
                cursor = step;
            }
            return found;
        }

        std::string Chars(const uint8_t *entry, const Shape &how)
        {
            uint16_t header = 0;
            if (entry == nullptr || !CopyRaw(entry + how.headerAt, &header, sizeof(header))) return {};
            const uint32_t len = static_cast<uint32_t>(header) >> how.shift;
            const bool wide = (header & 1u) != 0;
            if (len == 0 || len > 200) return {};
            const uint8_t *from = entry + how.charsAt;
            if (!wide)
            {
                char keep[208]{};
                if (!CopyRaw(from, keep, len)) return {};
                for (uint32_t i = 0; i < len; ++i)
                {
                    const auto one = static_cast<unsigned char>(keep[i]);
                    if (one < 0x20 || one > 0x7E) return {};
                }
                return std::string(keep, len);
            }
            wchar_t wideKeep[208]{};
            if (!CopyRaw(from, wideKeep, static_cast<size_t>(len) * sizeof(wchar_t))) return {};
            std::string out;
            out.reserve(len);
            for (uint32_t i = 0; i < len; ++i) out.push_back(wideKeep[i] < 0x80 ? static_cast<char>(wideKeep[i]) : '?');
            return out;
        }

        const uint8_t *Entry(uint32_t slot)
        {
            const uint32_t block = slot >> kBlockBits;
            const uint32_t step = slot & kBlockMask;
            if (blocks == nullptr || block >= 8192) return nullptr;
            uint8_t *base = nullptr;
            if (!CopyRaw(blocks + block, &base, sizeof(base)) || base == nullptr) return nullptr;
            return base + static_cast<size_t>(step) * 2u;
        }

        bool Sections(uint8_t *&data, size_t &size)
        {
            uint8_t *home = reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));
            if (home == nullptr) return false;
            const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(home);
            const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(home + dos->e_lfanew);
            const auto *cut = IMAGE_FIRST_SECTION(nt);
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
            {
                if (memcmp(cut[i].Name, ".data", 5) == 0)
                {
                    data = home + cut[i].VirtualAddress;
                    size = cut[i].Misc.VirtualSize;
                    return true;
                }
            }
            return false;
        }

        bool Code(uint8_t *&from, size_t &size)
        {
            uint8_t *home = reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));
            if (home == nullptr) return false;
            const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(home);
            const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(home + dos->e_lfanew);
            from = home + nt->OptionalHeader.BaseOfCode;
            size = nt->OptionalHeader.SizeOfCode;
            return size > 0;
        }

        bool InCode(const void *at)
        {
            static uint8_t *from = nullptr;
            static size_t size = 0;
            if (from == nullptr && !Code(from, size)) return false;
            const auto *spot = static_cast<const uint8_t *>(at);
            return spot >= from && spot < from + size;
        }

        int Chain(uint8_t *base, const Shape &how, int wanted, std::string &trail)
        {
            size_t step = 0;
            int seen = 0;
            for (; seen < wanted; ++seen)
            {
                uint16_t header = 0;
                if (!CopyRaw(base + step + how.headerAt, &header, sizeof(header))) break;
                const uint32_t len = static_cast<uint32_t>(header) >> how.shift;
                const bool wide = (header & 1u) != 0;
                if (len == 0 || len > 200) break;
                const std::string word = Chars(base + step, how);
                if (word.empty()) break;
                if (seen < 6)
                {
                    trail += word;
                    trail += ' ';
                }
                const size_t bytes = static_cast<size_t>(how.charsAt) + static_cast<size_t>(len) * (wide ? 2u : 1u);
                step += (bytes + 1u) & ~static_cast<size_t>(1u);
            }
            return seen;
        }

        bool HuntNames(uint8_t *data, size_t size, size_t &cursor)
        {
            const Shape tries[] = { { 0, 2, 6 }, { 4, 6, 1 }, { 0, 2, 1 } };
            for (size_t at = cursor; at + 8 <= size; at += 8)
            {
                uint8_t *first = nullptr;
                if (!CopyRaw(data + at, &first, sizeof(first)) || first == nullptr) continue;
                if (!Held(first, 128)) continue;
                for (const Shape &how : tries)
                {
                    if (Chars(first, how) != "None") continue;
                    std::string trail;
                    const int seen = Chain(first, how, 12, trail);
                    if (seen < 10) continue;
                    blocks = reinterpret_cast<uint8_t **>(data + at);
                    shape = how;
                    cursor = at + 8;
                    return true;
                }
            }
            return false;
        }

        bool BindObjectTable()
        {
            table = nullptr;
            objectCount = nullptr;
            if (!native_bindings::Ready()) return false;
            const auto root = native_bindings::Address(0xA0E68D0);
            const auto countAddress = native_bindings::Address(0xA0E68E4);
            int32_t count = 0;
            uint8_t **chart = nullptr;
            if (root < 0x10000 || countAddress < 0x10000 || !CopyRaw(reinterpret_cast<const void *>(countAddress), &count, sizeof(count)) || count < 16 ||
                count > 16000000 || !CopyRaw(reinterpret_cast<const void *>(root), &chart, sizeof(chart)) || chart == nullptr ||
                !Held(chart, sizeof(void *) * ((static_cast<size_t>(count) + 65535) / 65536)))
                return false;
            for (const int32_t width : { 24, 32, 16 })
            {
                const auto at = [&](int32_t index) -> const void * {
                    if (index < 0 || index >= count) return nullptr;
                    uint8_t *slab = nullptr;
                    const void *thing = nullptr;
                    if (!CopyRaw(chart + index / 65536, &slab, sizeof(slab)) || slab == nullptr ||
                        !CopyRaw(slab + static_cast<size_t>(index % 65536) * width, &thing, sizeof(thing)) || thing == nullptr || !Held(thing, 0x30))
                        return nullptr;
                    return thing;
                };
                const auto valid = [&](int32_t index) {
                    const void *thing = at(index);
                    if (thing == nullptr) return false;
                    int32_t ownIndex = -1, classIndex = -1;
                    if (!CopyRaw(static_cast<const uint8_t *>(thing) + 0xC, &ownIndex, sizeof(ownIndex)) || ownIndex != index) return false;
                    const void *type = ClassOf(thing);
                    if (type == nullptr || !Held(type, 0x30) || !CopyRaw(static_cast<const uint8_t *>(type) + 0xC, &classIndex, sizeof(classIndex)) || at(classIndex) != type)
                        return false;
                    const auto name = Label(thing);
                    const auto kind = Label(type);
                    return !name.empty() && name != "None" && !kind.empty() && kind != "None";
                };
                int good = 0;
                const int32_t first = std::min(count, 1024);
                for (int32_t index = 0; index < first && good < 16; ++index)
                    if (valid(index)) ++good;
                const int32_t step = std::max(1, (count - first + 255) / 256);
                for (int32_t index = first; index < count && good < 16; index += step)
                    if (valid(index)) ++good;
                if (good >= 16)
                {
                    uint8_t **afterChart = nullptr;
                    int32_t afterCount = 0;
                    if (!CopyRaw(reinterpret_cast<const void *>(root), &afterChart, sizeof(afterChart)) || afterChart != chart ||
                        !CopyRaw(reinterpret_cast<const void *>(countAddress), &afterCount, sizeof(afterCount)) || afterCount < count || afterCount > 16000000)
                        return false;
                    table = reinterpret_cast<uint8_t *>(root);
                    objectCount = reinterpret_cast<uint8_t *>(countAddress);
                    stride = width;
                    Trace("objects resolved at %p stride %d, live %d; names %p shape %d/%d/%d; %d identities verified", static_cast<void *>(table), stride, afterCount,
                          static_cast<void *>(blocks), shape.headerAt, shape.charsAt, shape.shift, good);
                    return true;
                }
            }
            return false;
        }

        bool BindPools(uint8_t *data, size_t size)
        {
            size_t cursor = 0;
            unsigned candidates = 0;
            while (HuntNames(data, size, cursor))
            {
                ++candidates;
                if (BindObjectTable() && FindClass("Actor") != nullptr && FindClass("Object") != nullptr) return true;
            }
            blocks = nullptr;
            table = nullptr;
            objectCount = nullptr;
            snag = candidates == 0 ? "names" : "objects";
            int32_t count = 0;
            const auto root = native_bindings::Address(0xA0E68D0);
            const auto countAddress = native_bindings::Address(0xA0E68E4);
            if (countAddress >= 0x10000) CopyRaw(reinterpret_cast<const void *>(countAddress), &count, sizeof(count));
            Trace("object/name binding pending: %u name candidates, root %p, live %d", candidates, reinterpret_cast<void *>(root), count);
            return false;
        }

        int Score(const void *type, const Fields &how, std::string *trail)
        {
            const void *prop = Grab(type, how.listAt);
            int good = 0;
            const void *visited[96]{};
            int32_t ownerAt = -1, rootAt = -1;
            for (int guard = 0; guard < 96 && prop != nullptr; ++guard)
            {
                if (std::find(visited, visited + good, prop) != visited + good) return 0;
                if (!Held(prop, 0x60)) break;
                uint32_t pair[2]{};
                int32_t at = -1;
                if (!CopyRaw(static_cast<const uint8_t *>(prop) + how.nameAt, pair, sizeof(pair)) || !CopyRaw(static_cast<const uint8_t *>(prop) + how.offsetAt, &at, sizeof(at)))
                    break;
                const std::string word = Word(pair[0]);
                if (word.empty() || at < 0 || at > 0x8000) break;
                if (word == "Owner") ownerAt = at;
                if (word == "RootComponent") rootAt = at;
                if (trail != nullptr && good < 5)
                {
                    char note[96]{};
                    _snprintf_s(note, sizeof(note), _TRUNCATE, "%s+0x%X ", word.c_str(), at);
                    *trail += note;
                }
                visited[good++] = prop;
                prop = Grab(prop, how.nextAt);
            }
            return ownerAt > 0 && rootAt > 0 && ownerAt != rootAt && (ownerAt & 7) == 0 && (rootAt & 7) == 0 ? good : 0;
        }

        bool HuntFields()
        {
            const void *actor = FindClass("Actor");
            if (actor == nullptr)
            {
                Trace("class Actor not found for field probe");
                return false;
            }
            const int32_t lists[] = { 0x50, 0x48, 0x58 };
            const Fields shapes[] = { { 0, 0x20, 0x28, 0x4C }, { 0, 0x18, 0x20, 0x44 }, { 0, 0x28, 0x30, 0x54 }, { 0, 0x20, 0x28, 0x44 } };
            Fields best;
            int top = 0;
            std::string keep;
            for (const int32_t listAt : lists)
            {
                for (const Fields &guess : shapes)
                {
                    Fields how = guess;
                    how.listAt = listAt;
                    std::string trail;
                    const int good = Score(actor, how, &trail);
                    if (good > top)
                    {
                        top = good;
                        best = how;
                        keep = trail;
                    }
                }
            }
            if (top < 8)
            {
                for (int32_t listAt = 0x40; listAt <= 0x88; listAt += 8)
                    for (int32_t nextAt = 0x10; nextAt <= 0x40; nextAt += 8)
                        for (int32_t nameAt = 0x18; nameAt <= 0x48; nameAt += 8)
                            for (int32_t offsetAt = 0x30; offsetAt <= 0x58; offsetAt += 4)
                            {
                                if (nextAt == nameAt || (offsetAt >= nextAt && offsetAt < nextAt + 8) || (offsetAt >= nameAt && offsetAt < nameAt + 8)) continue;
                                const Fields how{ listAt, nextAt, nameAt, offsetAt };
                                std::string trail;
                                const int good = Score(actor, how, &trail);
                                if (good > top)
                                {
                                    top = good;
                                    best = how;
                                    keep = trail;
                                }
                            }
            }
            if (top < 8)
            {
                Trace("field layout not found, best %d", top);
                return false;
            }
            fields = best;
            Trace("fields list +0x%X next +0x%X name +0x%X offset +0x%X (%d ok): %s", fields.listAt, fields.nextAt, fields.nameAt, fields.offsetAt, top, keep.c_str());
            return true;
        }

        std::vector<const void *> Verbs(const std::string &name)
        {
            std::vector<const void *> out;
            {
                std::shared_lock<std::shared_mutex> hold(bookGate);
                const auto range = book.equal_range(name);
                for (auto it = range.first; it != range.second; ++it) out.push_back(it->second);
                if (out.empty())
                {
                    for (const auto &one : book)
                        if (text::Same(one.first, name)) out.push_back(one.second);
                }
            }
            if (out.empty()) Doubt(name);
            return out;
        }

        const void *Verb(const char *name)
        {
            const std::vector<const void *> found = Verbs(name);
            return found.empty() ? nullptr : found.front();
        }

        const void *One(const void *type)
        {
            const void *spare = nullptr;
            const void *heir = nullptr;
            const int32_t count = Total();
            for (int32_t index = 0; index < count; ++index)
            {
                const void *thing = At(index);
                if (thing == nullptr) continue;
                const void *own = ClassOf(thing);
                if (own != type)
                {
                    if (heir == nullptr && own != nullptr && Kin(own, type) && Label(thing).rfind("Default__", 0) != 0) heir = thing;
                    continue;
                }
                if (Label(thing).rfind("Default__", 0) != 0) return thing;
                spare = thing;
            }
            return heir != nullptr ? heir : spare;
        }

        int Blame(EXCEPTION_POINTERS *info)
        {
            if (proven && info != nullptr && info->ExceptionRecord != nullptr)
            {
                const EXCEPTION_RECORD &rec = *info->ExceptionRecord;
                const void *address = rec.NumberParameters >= 2 ? reinterpret_cast<const void *>(rec.ExceptionInformation[1]) : nullptr;
                Trace("call faulted: %08X at %p, address %p", rec.ExceptionCode, rec.ExceptionAddress, address);
            }
            return EXCEPTION_EXECUTE_HANDLER;
        }

        bool Fire(Thunk hand, const void *thing, void *frame, void *fruit)
        {
            __try
            {
                hand(thing, frame, fruit);
                return true;
            }
            __except (Blame(GetExceptionInformation()))
            {
                return false;
            }
        }

        Thunk Hand(const void *verb)
        {
            if (verb == nullptr || rig.thunkAt < 0) return nullptr;
            const void *hand = Grab(verb, rig.thunkAt);
            return InCode(hand) ? reinterpret_cast<Thunk>(const_cast<void *>(hand)) : nullptr;
        }

        struct OutRec
        {
            const void *prop = nullptr;
            uint8_t *at = nullptr;
            OutRec *next = nullptr;
        };

        bool Drive(const void *verb, const void *thing, void *parms, void *fruit)
        {
            Thunk hand = Hand(verb);
            if (hand == nullptr || rig.chainAt < 0) return false;
            const void *chain = Grab(verb, fields.listAt);
            const int32_t flagsAt = fields.offsetAt - 0x0C;
            const int32_t sizeAt = fields.offsetAt - 0x10;
            OutRec recs[32]{};
            int made = 0;
            void *result = fruit;
            int32_t resultSize = 0;
            const void *prop = chain;
            for (int guard = 0; guard < 64 && prop != nullptr; ++guard)
            {
                uint64_t flags = 0;
                int32_t at = -1;
                if (!CopyRaw(static_cast<const uint8_t *>(prop) + flagsAt, &flags, sizeof(flags)) || !CopyRaw(static_cast<const uint8_t *>(prop) + fields.offsetAt, &at, sizeof(at)) ||
                    at < 0 || (flags & kParm) == 0)
                    break;
                if ((flags & kOutParm) != 0 && made < 32)
                {
                    recs[made].prop = prop;
                    recs[made].at = static_cast<uint8_t *>(parms) + at;
                    if (made > 0) recs[made - 1].next = &recs[made];
                    ++made;
                }
                if ((flags & kReturnParm) != 0)
                {
                    result = static_cast<uint8_t *>(parms) + at;
                    CopyRaw(static_cast<const uint8_t *>(prop) + sizeAt, &resultSize, sizeof(resultSize));
                }
                prop = Grab(prop, fields.nextAt);
            }
            alignas(16) uint8_t frame[0x140]{};
            *reinterpret_cast<void **>(frame + rig.localsAt) = parms;
            *reinterpret_cast<const void **>(frame + rig.chainAt) = chain;
            *reinterpret_cast<OutRec **>(frame + rig.chainAt - 8) = made > 0 ? recs : nullptr;
            const bool went = Fire(hand, thing, frame, result);
            if (went && result != fruit && fruit != nullptr && resultSize > 0) memcpy(fruit, result, std::min<size_t>(static_cast<size_t>(resultSize), 0x40));
            return went;
        }

        bool Facets(const void *klass, int32_t at, const void *owner, int32_t &shift)
        {
            const uint8_t *seat = static_cast<const uint8_t *>(klass) + at;
            const uint8_t *data = static_cast<const uint8_t *>(Grab(klass, at));
            int32_t num = 0;
            int32_t max = 0;
            if (data == nullptr || !CopyRaw(seat + 8, &num, sizeof(num)) || !CopyRaw(seat + 12, &max, sizeof(max)) || num <= 0 || num > 64 || max < num || max > 256 ||
                !Held(data, static_cast<size_t>(num) * 16))
                return false;
            for (int32_t index = 0; index < num; ++index)
            {
                const void *face = Grab(data, index * 16);
                int32_t offset = -1;
                if (face == nullptr || !Held(face, 0x30) || Label(ClassOf(face)) != "Class" || !CopyRaw(data + index * 16 + 8, &offset, sizeof(offset)) || offset < 0 ||
                    offset > 0x4000)
                    return false;
                if (face == owner) shift = offset;
            }
            return true;
        }

        void HuntThunk()
        {
            const char *pick[] = { "Add_IntInt", "XSetFullGodMode", "XSetGodMode", "K2_GetActorLocation", "XTeleportTo", "XSkipTimeHours", "XStartSleep" };
            reins.clear();
            for (int32_t at = 0x88; at <= 0x108; at += 8)
            {
                std::vector<const void *> hands;
                for (const char *name : pick)
                {
                    const void *verb = Verb(name);
                    if (verb == nullptr) continue;
                    const void *hand = Grab(verb, at);
                    if (!InCode(hand))
                    {
                        hands.clear();
                        break;
                    }
                    hands.push_back(hand);
                }
                if (hands.size() < 3) continue;
                std::sort(hands.begin(), hands.end());
                hands.erase(std::unique(hands.begin(), hands.end()), hands.end());
                if (hands.size() >= 2) reins.push_back(at);
            }
            if (reins.empty()) Trace("thunk field not found");
        }

        void HuntFrame()
        {
            const void *sum = Verb("Add_IntInt");
            const void *host = sum != nullptr ? One(FindClass("KismetMathLibrary")) : nullptr;
            if (sum == nullptr || host == nullptr)
            {
                Trace("frame: math probe unavailable");
                return;
            }
            for (const int32_t at : reins)
            {
                rig.thunkAt = at;
                if (Hand(sum) == nullptr) continue;
                for (int32_t chain = 0x40; chain <= 0xC8; chain += 8)
                {
                    rig.chainAt = chain;
                    alignas(16) int32_t parms[4] = { 17, 25, 0, 0 };
                    alignas(16) int64_t fruit = 0;
                    if (!Drive(sum, host, parms, &fruit)) continue;
                    if (static_cast<int32_t>(fruit) == 42 || parms[2] == 42)
                    {
                        proven = true;
                        Trace("frame locked: thunk +0x%X locals +0x%X chain +0x%X", at, rig.localsAt, chain);
                        return;
                    }
                }
            }
            rig.thunkAt = -1;
            rig.chainAt = -1;
            Trace("frame layout not found");
        }
    }

    bool Copy(const void *at, void *into, size_t bytes)
    {
        return CopyRaw(at, into, bytes);
    }

#if defined(ZSDK_TESTS)
    void Pose(uint8_t **names, uint8_t *objects, uint8_t *count, int32_t width)
    {
        blocks = names;
        table = objects;
        objectCount = count;
        stride = width;
        shape = Shape{};
        fields = Fields{};
        facetsAt = -1;
        bound = names != nullptr && objects != nullptr;
        ClearHosts();
        Survey();
    }

    void Poise()
    {
        proven = bound.load();
    }
#endif

    void Survey()
    {
        std::vector<Patch> fresh;
        fresh.reserve(4096);
        Walk(fresh, Stacks());
        std::unique_lock<std::shared_mutex> hold(plan);
        live.swap(fresh);
        era.fetch_add(1, std::memory_order_release);
    }

    bool Held(const void *spot, size_t bytes)
    {
        const uintptr_t at = reinterpret_cast<uintptr_t>(spot);
        if (at < 0x10000 || bytes == 0) return false;
        struct Recent
        {
            Patch span{ 1, 0 };
            uint32_t era = 0;
        };
        static thread_local Recent recent;
        const uint32_t now = era.load(std::memory_order_acquire);
        if (recent.era == now && at >= recent.span.from && at + bytes <= recent.span.upto) return true;
        std::shared_lock<std::shared_mutex> hold(plan);
        const auto seat = std::upper_bound(live.begin(), live.end(), at, [](uintptr_t value, const Patch &patch) { return value < patch.from; });
        if (seat == live.begin()) return false;
        const Patch &patch = *(seat - 1);
        if (at < patch.from || at + bytes > patch.upto) return false;
        recent.span = patch;
        recent.era = now;
        return true;
    }

    const void *Grab(const void *thing, int32_t at)
    {
        const void *value = nullptr;
        if (thing == nullptr || !CopyRaw(static_cast<const uint8_t *>(thing) + at, &value, sizeof(value))) return nullptr;
        return value;
    }

    std::string Word(uint32_t slot)
    {
        return Chars(Entry(slot), shape);
    }

    const uint8_t *Item(int32_t index)
    {
        if (table == nullptr || index < 0) return nullptr;
        uint8_t **chart = nullptr;
        uint8_t *slab = nullptr;
        if (!CopyRaw(table, &chart, sizeof(chart)) || chart == nullptr || !CopyRaw(chart + index / 65536, &slab, sizeof(slab)) || slab == nullptr) return nullptr;
        return slab + static_cast<size_t>(index % 65536) * static_cast<size_t>(stride);
    }

    int32_t Total()
    {
        int32_t count = 0;
        if (table != nullptr && objectCount != nullptr) CopyRaw(objectCount, &count, sizeof(count));
        return count >= 0 && count <= 16000000 ? count : 0;
    }

    int32_t Stride()
    {
        return stride;
    }

    const void *At(int32_t index)
    {
        const uint8_t *slot = Item(index);
        uint8_t head[kItemFlagsAt + sizeof(uint32_t)]{};
        if (slot == nullptr || !CopyRaw(slot, head, sizeof(head))) return nullptr;
        const void *thing = nullptr;
        uint32_t flags = 0;
        memcpy(&thing, head, sizeof(thing));
        memcpy(&flags, head + kItemFlagsAt, sizeof(flags));
        if (thing == nullptr || (flags & kItemGone) != 0 || !Held(thing, 0x30)) return nullptr;
        return thing;
    }

    const void *ClassOf(const void *thing)
    {
        return Grab(thing, kClassAt);
    }

    const void *OuterOf(const void *thing)
    {
        return Grab(thing, kOuterAt);
    }

    std::string Label(const void *thing)
    {
        uint32_t pair[2]{};
        if (thing == nullptr || !CopyRaw(static_cast<const uint8_t *>(thing) + kNameAt, pair, sizeof(pair))) return {};
        std::string word = Word(pair[0]);
        if (word.empty() || pair[1] == 0) return word;
        return word + "_" + std::to_string(pair[1] - 1);
    }

    std::string Path(const void *thing)
    {
        std::string trail = Label(thing);
        const void *walk = OuterOf(thing);
        for (int depth = 0; depth < 8 && walk != nullptr; ++depth)
        {
            const std::string step = Label(walk);
            if (step.empty()) break;
            trail = step + "." + trail;
            walk = OuterOf(walk);
        }
        return trail;
    }

    bool Kin(const void *type, const void *base)
    {
        const void *walk = type;
        for (int depth = 0; depth < 40 && walk != nullptr; ++depth)
        {
            if (walk == base) return true;
            walk = Grab(walk, kSuperAt);
        }
        return false;
    }

    bool Inherits(const void *type, const char *name)
    {
        const void *walk = type;
        for (int depth = 0; depth < 24 && walk != nullptr; ++depth)
        {
            if (Label(walk) == name) return true;
            walk = Grab(walk, kSuperAt);
        }
        return false;
    }

    bool Standing(const void *thing, const void *type)
    {
        int32_t seat = -1;
        if (thing == nullptr || type == nullptr || !Held(thing, 0x30) || ClassOf(thing) != type || !CopyRaw(static_cast<const uint8_t *>(thing) + 0x0C, &seat, sizeof(seat)) ||
            seat < 0)
            return false;
        return At(seat) == thing;
    }

    const void *FindClass(const char *className)
    {
        const int32_t count = Total();
        for (int32_t index = 0; index < count; ++index)
        {
            const void *thing = At(index);
            if (thing == nullptr || Label(thing) != className) continue;
            const std::string kind = Label(ClassOf(thing));
            if (kind == "Class" || kind == "BlueprintGeneratedClass") return thing;
        }
        return nullptr;
    }

    const void *FindLive(const char *baseClass)
    {
        const int32_t count = Total();
        for (int32_t index = count - 1; index >= 0; --index)
        {
            const void *thing = At(index);
            if (thing == nullptr) continue;
            const void *type = ClassOf(thing);
            if (type == nullptr || !Inherits(type, baseClass)) continue;
            const std::string name = Label(thing);
            if (!name.empty() && name.rfind("Default__", 0) != 0) return thing;
        }
        return nullptr;
    }

    int32_t PropertyOffset(const void *type, const char *field)
    {
        if (!bound || type == nullptr || field == nullptr) return -1;
        const void *walk = type;
        for (int depth = 0; depth < 24 && walk != nullptr; ++depth)
        {
            const void *prop = Grab(walk, fields.listAt);
            for (int guard = 0; guard < 4096 && prop != nullptr; ++guard)
            {
                uint32_t pair[2]{};
                if (CopyRaw(static_cast<const uint8_t *>(prop) + fields.nameAt, pair, sizeof(pair)) && Word(pair[0]) == field)
                {
                    int32_t at = 0;
                    if (CopyRaw(static_cast<const uint8_t *>(prop) + fields.offsetAt, &at, sizeof(at)) && at >= 0 && at < 0x8000) return at;
                }
                prop = Grab(prop, fields.nextAt);
            }
            walk = Grab(walk, kSuperAt);
        }
        return -1;
    }

    int32_t Bulk(const void *type)
    {
        int32_t size = 0;
        if (type == nullptr || !CopyRaw(static_cast<const uint8_t *>(type) + fields.listAt + 8, &size, sizeof(size))) return 0;
        return size > 0 && size < 0x20000 ? size : 0;
    }

    void Restock()
    {
        std::unordered_multimap<std::string, const void *> fresh;
        const int32_t count = Total();
        for (int32_t index = 0; index < count; ++index)
        {
            const void *thing = At(index);
            if (thing == nullptr) continue;
            const void *type = ClassOf(thing);
            if (type != nullptr && Label(type) == "Function") fresh.emplace(Label(thing), thing);
        }
        const size_t entries = fresh.size();
        {
            std::unique_lock<std::shared_mutex> hold(bookGate);
            book.swap(fresh);
        }
        swept = GetTickCount();
        doubt = false;
        bookEpoch.fetch_add(1);
        booked = true;
        Trace("function book: %zu entries over %d objects", entries, count);
    }

    bool Refresh()
    {
        if (!doubt.load() || !booked.load() || dusk.load()) return false;
        if (GetTickCount() - swept.load() < kRefreshMs) return false;
        Restock();
        return true;
    }

    bool Stale()
    {
        const DWORD at = swept.load();
        return at == 0 || GetTickCount() - at >= kStaleMs;
    }

    void Want()
    {
        bookWanted = true;
    }

    bool Wanted()
    {
        return bookWanted.load();
    }

    bool Serve()
    {
        if (!bookWanted.exchange(false) || dusk.load()) return false;
        Survey();
        Restock();
        return true;
    }

    bool Booked()
    {
        return booked.load();
    }

    uint32_t BookEpoch()
    {
        return bookEpoch.load();
    }

    std::vector<const void *> Exact(const std::string &name)
    {
        std::vector<const void *> out;
        {
            std::shared_lock<std::shared_mutex> hold(bookGate);
            const auto range = book.equal_range(name);
            for (auto it = range.first; it != range.second; ++it) out.push_back(it->second);
        }
        if (out.empty()) Doubt(name);
        return out;
    }

    const void *Host(const void *type)
    {
        std::lock_guard<std::mutex> hold(stallGate);
        Desk &desk = stall[type];
        if (Standing(desk.host, ClassOf(desk.host)) && !ue::Doomed(desk.host) && (ClassOf(desk.host) == type || Kin(ClassOf(desk.host), type))) return desk.host;
        const DWORD now = GetTickCount();
        if (desk.tried != 0 && now - desk.tried < kHostRetryMs) return nullptr;
        desk.tried = now;
        desk.host = One(type);
        if (desk.host && ue::Doomed(desk.host)) desk.host = nullptr;
        if (desk.host && Label(desk.host).rfind("Default__", 0) == 0 && !Inherits(type, "BlueprintFunctionLibrary"))
        {
            const std::string name = Label(type);
            Rare(("host " + name).c_str(), "engine: host for %s not ready", name.c_str());
            desk.host = nullptr;
        }
        return desk.host;
    }

    void ClearHosts()
    {
        std::lock_guard<std::mutex> hold(stallGate);
        stall.clear();
    }

    Rode Ride(const void *verb, const void *thing, void *parms, void *fruit)
    {
        if (dusk) return Rode::Refused;
        if (!thread::InPump())
        {
            Rare("engine off pump", "engine: a native call was refused outside the game thread pump");
            return Rode::Refused;
        }
        return Drive(verb, thing, parms, fruit) ? Rode::Sent : Rode::Faulted;
    }

    const void *Facet(const void *thing, const void *verb)
    {
        const void *owner = OuterOf(verb);
        const void *type = ClassOf(thing);
        if (owner == nullptr || type == nullptr || Kin(type, owner)) return thing;
        const void *walk = type;
        for (int depth = 0; depth < 40 && walk != nullptr; ++depth)
        {
            int32_t shift = -1;
            if (facetsAt >= 0) Facets(walk, facetsAt, owner, shift);
            else
            {
                for (int32_t at = 0xB0; at < 0x300; at += 8)
                {
                    if (Facets(walk, at, owner, shift))
                    {
                        facetsAt = at;
                        break;
                    }
                }
            }
            if (shift >= 0) return static_cast<const uint8_t *>(thing) + shift;
            walk = Grab(walk, kSuperAt);
        }
        return thing;
    }

    bool Analyze(const void *fn, Sign &out)
    {
        out.owner = Label(OuterOf(fn));
        out.bytes = Bulk(fn);
        out.params.clear();
        const int32_t flagsAt = fields.offsetAt - 0x0C;
        const int32_t sizeAt = fields.offsetAt - 0x10;
        const void *prop = Grab(fn, fields.listAt);
        for (int guard = 0; guard < 64 && prop != nullptr; ++guard)
        {
            Param one;
            uint32_t pair[2]{};
            uint32_t kindName[2]{};
            const void *kind = Grab(prop, 0x08);
            if (!Held(prop, 0x60) || !CopyRaw(static_cast<const uint8_t *>(prop) + flagsAt, &one.flags, sizeof(one.flags)) ||
                !CopyRaw(static_cast<const uint8_t *>(prop) + fields.offsetAt, &one.at, sizeof(one.at)) || !CopyRaw(static_cast<const uint8_t *>(prop) + sizeAt, &one.size, sizeof(one.size)) ||
                !CopyRaw(static_cast<const uint8_t *>(prop) + fields.nameAt, pair, sizeof(pair)) || one.at < 0)
                break;
            if (kind != nullptr) CopyRaw(kind, kindName, sizeof(kindName));
            one.name = Word(pair[0]);
            one.type = Word(kindName[0]);
            one.field = prop;
            if ((one.flags & kParm) != 0) out.params.push_back(one);
            prop = Grab(prop, fields.nextAt);
        }
        if (out.bytes <= 0)
        {
            for (const Param &one : out.params) out.bytes = std::max(out.bytes, one.at + one.size);
            out.bytes = (out.bytes + 15) & ~15;
        }
        return true;
    }

    bool Choices(const Param &one, std::vector<Pick> &out)
    {
        out.clear();
        if (one.field == nullptr || (one.type != "EnumProperty" && one.type != "ByteProperty")) return false;
        const int32_t tail = fields.offsetAt + 0x2C;
        const void *type = Grab(one.field, one.type == "EnumProperty" ? tail + 8 : tail);
        const std::string kind = type != nullptr && Held(type, kNamesAt + 0x10) ? Label(ClassOf(type)) : std::string();
        if (kind != "Enum" && kind != "UserDefinedEnum") return false;
        const std::string owner = Label(type);
        const uint8_t *seat = static_cast<const uint8_t *>(type) + kNamesAt;
        const uint8_t *data = static_cast<const uint8_t *>(Grab(type, kNamesAt));
        int32_t num = 0;
        int32_t max = 0;
        if (owner.empty() || data == nullptr || !CopyRaw(seat + 8, &num, sizeof(num)) || !CopyRaw(seat + 12, &max, sizeof(max)) || num <= 0 || num > 1024 || max < num ||
            !Held(data, static_cast<size_t>(num) * 16))
            return false;
        for (int32_t index = 0; index < num; ++index)
        {
            uint32_t pair[2]{};
            int64_t value = 0;
            const uint8_t *entry = data + static_cast<size_t>(index) * 16;
            std::string name = CopyRaw(entry, pair, sizeof(pair)) && CopyRaw(entry + 8, &value, sizeof(value)) ? Word(pair[0]) : std::string();
            if (!name.empty() && pair[1] != 0) name += "_" + std::to_string(pair[1] - 1);
            const size_t cut = name.rfind("::");
            if (name.empty() || (cut != std::string::npos && name.compare(0, cut, owner) != 0))
            {
                out.clear();
                return false;
            }
            out.push_back({ name, value });
        }
        return true;
    }

    bool Bind()
    {
        if (bound) return true;
        std::lock_guard<std::recursive_mutex> hold(work);
        if (bound) return true;
        failed = false;
        if (dusk) return false;
        if (!native_bindings::Initialize())
        {
            snag = "patterns";
            failed = native_bindings::State() == native_bindings::Status::Rejected;
            return false;
        }
        Survey();
        if (!poolsReady)
        {
            uint8_t *data = nullptr;
            size_t size = 0;
            if (!Sections(data, size))
            {
                Trace("no .data section in the game module");
                snag = "sections";
                failed = true;
                return false;
            }
            if (!BindPools(data, size)) return false;
            poolsReady = true;
        }
        if (!HuntFields())
        {
            snag = "fields";
            return false;
        }
        bound = true;
        snag = "thunk";
        return true;
    }

    bool Wire()
    {
        std::lock_guard<std::recursive_mutex> hold(work);
        if (proven || dusk || !bound) return proven;
        if (!thread::InPump())
        {
            Rare("engine wire off pump", "engine: wiring was refused outside the game thread pump");
            return false;
        }
        if (Stale())
        {
            Want();
            return false;
        }
        HuntThunk();
        if (!reins.empty()) HuntFrame();
        snag = proven ? "" : (reins.empty() ? "thunk" : "frame");
        return proven;
    }

    void Dusk()
    {
        dusk.store(true);
        proven.store(false);
    }

    bool Ready()
    {
        return bound && proven && !dusk;
    }

    bool Bound()
    {
        return bound.load();
    }

    bool Dusked()
    {
        return dusk.load();
    }

    bool Rejected()
    {
        return native_bindings::State() == native_bindings::Status::Rejected;
    }

    const char *Snag()
    {
        return snag;
    }
}
