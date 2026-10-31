#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/native_bindings_detail.h"
#include "../src/detail/quit.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

namespace zsdk::detail::native_bindings::detail
{
#include "../src/detail/native_bindings_catalog.inc"
}

namespace
{
    using check::That;
    namespace nb = zsdk::detail::native_bindings::detail;

    constexpr std::uint32_t kRoot = 0xA0E68D0u;
    constexpr std::uint32_t kCount = 0xA0E68E4u;

    struct Exe
    {
        std::vector<std::uint8_t> file;
        std::vector<std::vector<std::uint8_t>> copies;
        std::vector<nb::Section> sections;
        std::uint32_t image = 0;
        std::uint32_t stamp = 0;
    };

    template<class T> bool Take(const std::vector<std::uint8_t> &file, std::size_t at, T &out)
    {
        if (at > file.size() || sizeof(T) > file.size() - at) return false;
        std::memcpy(&out, file.data() + at, sizeof(T));
        return true;
    }

    bool Load(const std::filesystem::path &path, Exe &exe)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        exe.file.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        std::uint32_t lfanew = 0;
        std::uint16_t count = 0;
        std::uint16_t optional = 0;
        if (!Take(exe.file, 0x3C, lfanew) || !Take(exe.file, lfanew + 6, count) || !Take(exe.file, lfanew + 20, optional)) return false;
        if (!Take(exe.file, lfanew + 8, exe.stamp) || !Take(exe.file, lfanew + 24 + 56, exe.image)) return false;
        const std::size_t first = lfanew + 24 + optional;
        exe.copies.resize(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            const std::size_t at = first + 40u * i;
            std::uint32_t virtualSize = 0, rva = 0, rawSize = 0, rawAt = 0, flags = 0;
            if (!Take(exe.file, at + 8, virtualSize) || !Take(exe.file, at + 12, rva) || !Take(exe.file, at + 16, rawSize) || !Take(exe.file, at + 20, rawAt) || !Take(exe.file, at + 36, flags))
                return false;
            if (!virtualSize) continue;
            nb::Section s{ rva, virtualSize, (flags & IMAGE_SCN_MEM_EXECUTE) != 0, (flags & IMAGE_SCN_MEM_READ) != 0 };
            if (s.executable)
            {
                const std::size_t take = std::min<std::size_t>(virtualSize, rawSize);
                if (rawAt > exe.file.size() || take > exe.file.size() - rawAt) return false;
                exe.copies[i].assign(exe.file.begin() + rawAt, exe.file.begin() + rawAt + take);
                exe.copies[i].resize(virtualSize, 0);
                s.bytes = exe.copies[i].data();
            }
            exe.sections.push_back(s);
        }
        return !exe.sections.empty();
    }

    struct Near
    {
        int one = 0;
        int two = 0;
    };

    Near Neighbours(const Exe &exe, const nb::Pattern &p)
    {
        Near found;
        for (const nb::Section &s : exe.sections)
        {
            if (!s.executable || s.size < p.size) continue;
            for (std::uint32_t at = 0; at + p.size <= s.size; ++at)
            {
                int off = 0;
                for (std::uint32_t j = 0; j < p.size && off <= 2; ++j)
                    if (p.mask[j] && s.bytes[at + j] != p.bytes[j]) ++off;
                if (off == 1) ++found.one;
                if (off == 2) ++found.two;
            }
        }
        return found;
    }

    std::uint32_t Bound(const nb::Result &out, std::uint32_t key)
    {
        for (const nb::Binding &b : out.bindings)
            if (b.key == key) return b.rva;
        return 0;
    }

    std::string Hex(std::uint32_t value)
    {
        char text[16]{};
        std::snprintf(text, sizeof(text), "0x%X", value);
        return text;
    }

    std::string Value(const std::string &json, const std::string &key)
    {
        const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch hit;
        return std::regex_search(json, hit, pattern) ? hit[1].str() : std::string();
    }

    std::vector<int> Masked(const char *bytes, int from)
    {
        std::vector<int> out;
        if (!zsdk::guard::Hex(bytes, out)) return {};
        for (int j = from; j < from + 4 && j < static_cast<int>(out.size()); ++j) out[static_cast<size_t>(j)] = -1;
        return out;
    }

    bool Fits(const std::uint8_t *at, const std::vector<int> &want)
    {
        for (size_t j = 0; j < want.size(); ++j)
            if (want[j] >= 0 && at[j] != static_cast<std::uint8_t>(want[j])) return false;
        return true;
    }

    std::int32_t Disp(const std::uint8_t *at)
    {
        std::int32_t value = 0;
        std::memcpy(&value, at, sizeof(value));
        return value;
    }

    struct Exit
    {
        int found = 0;
        std::uint32_t anchor = 0;
        std::uint32_t store = 0;
        std::uint32_t flag = 0;
        std::uint32_t stored = 0;
    };

    bool Holds(const Exe &exe, const zsdk::guard::Guard &g)
    {
        std::vector<int> want;
        if (!zsdk::guard::Hex(g.bytes, want)) return false;
        for (const nb::Section &s : exe.sections)
            if (s.bytes && g.rva >= s.rva && g.rva - s.rva < s.size) return want.size() <= s.size - (g.rva - s.rva) && Fits(s.bytes + (g.rva - s.rva), want);
        return false;
    }

    Exit FindExit(const Exe &exe, const zsdk::detail::quit::Exit &set)
    {
        Exit out;
        const zsdk::guard::Guard *list = set.set->guards;
        const std::vector<int> head = Masked(list[0].bytes, set.tested->offset);
        const std::vector<int> tail = Masked(list[1].bytes, set.flag->offset);
        const std::uint32_t gap = list[1].rva - list[0].rva;
        if (head.empty() || tail.empty() || head[0] < 0) return out;
        for (const nb::Section &s : exe.sections)
        {
            if (!s.executable || !s.bytes || s.size < gap + tail.size()) continue;
            for (std::uint32_t at = 0; at + gap + tail.size() <= s.size; ++at)
            {
                if (s.bytes[at] != static_cast<std::uint8_t>(head[0]) || !Fits(s.bytes + at, head)) continue;
                ++out.found;
                out.anchor = s.rva + at;
                out.flag = s.rva + at + static_cast<std::uint32_t>(set.tested->offset + 5) + static_cast<std::uint32_t>(Disp(s.bytes + at + set.tested->offset));
                if (Fits(s.bytes + at + gap, tail))
                {
                    out.store = s.rva + at + gap;
                    out.stored = out.store + 7u + static_cast<std::uint32_t>(Disp(s.bytes + at + gap + set.flag->offset));
                }
            }
        }
        return out;
    }
}

int main()
{
    That(kCount - kRoot == 0x14, "the two catalog keys are FUObjectArray.ObjObjects.Objects and NumElements, 0x14 apart");
    const std::string list = scan::Env("ZSDK_EXE", "");
    if (list.empty())
    {
        std::printf("exescan: no exe given (ZSDK_EXE, one path or several split by ';'), the real image scan is not run\n");
        return check::Finish("exescan");
    }
    size_t from = 0;
    while (from <= list.size())
    {
        const size_t cut = list.find(';', from);
        const std::string one = list.substr(from, cut == std::string::npos ? std::string::npos : cut - from);
        from = cut == std::string::npos ? list.size() + 1 : cut + 1;
        if (one.empty()) continue;
        Exe exe;
        const bool loaded = Load(std::filesystem::path(one), exe);
        That(loaded, "the exe is readable as a PE image: " + one);
        if (!loaded) continue;
        nb::Result out;
        const bool ok = nb::Resolve(exe.file.data(), exe.image, exe.sections, nb::kPatterns, out, nb::kLinks);
        const std::uint32_t root = Bound(out, kRoot);
        const std::uint32_t count = Bound(out, kCount);
        std::printf("exescan: %s stamp 0x%08X image 0x%X: %s, %zu of %zu rows matched, root %s, count %s\n", one.c_str(), exe.stamp, exe.image, ok ? "resolved" : "refused", out.matched, out.patterns,
                    Hex(root).c_str(), Hex(count).c_str());
        That(ok && out.failure == nb::Failure::None && out.matched == std::size(nb::kPatterns), "the SDK resolver matches every catalog row exactly once in " + one);
        That(root && count == root + 0x14, "the object count lands 0x14 after the object root, the FUObjectArray layout, in " + one);
        if (exe.stamp == zsdk::guard::k61bc1e03.stamp && exe.image == zsdk::guard::k61bc1e03.imageSize)
            That(root == kRoot && count == kCount, "on build 61bc1e03 the resolver gives the wired addresses 0xA0E68D0 and 0xA0E68E4");
        const zsdk::guard::Build *known = nullptr;
        for (const zsdk::guard::Build *build : zsdk::guard::kBuilds)
            if (exe.stamp == build->stamp && exe.image == build->imageSize) known = build;
        if (known)
            That(root == known->objectsRoot && count == known->objectsCount,
                 std::string("on build ") + known->id + " the resolver gives the object root " + Hex(known->objectsRoot) + " and count " + Hex(known->objectsCount) + " of the build row");
        const std::string build = known ? std::string("build ") + known->id : std::string("a build this ZoneSDK does not know, forecast only");
        int on = 0;
        const zsdk::detail::quit::Exit *armed = nullptr;
        for (const zsdk::detail::quit::Exit &set : zsdk::detail::quit::kExits)
        {
            const Exit leave = FindExit(exe, set);
            const bool bytes = Holds(exe, set.set->guards[0]) && Holds(exe, set.set->guards[1]);
            const bool comes = bytes && exe.image == set.set->build->imageSize;
            if (comes)
            {
                ++on;
                armed = &set;
            }
            std::printf("exescan:   exit set %s: masked anchor %d place%s, RequestEngineExit %s, store %s, GIsRequestingExit %s by the test and %s by the store; guards %s, image %s -> %s; %s\n", set.set->name,
                        leave.found, leave.found == 1 ? "" : "s", Hex(leave.anchor).c_str(), Hex(leave.store).c_str(), Hex(leave.flag).c_str(), Hex(leave.stored).c_str(), bytes ? "match" : "differ",
                        exe.image == set.set->build->imageSize ? "ok" : "differs", comes ? "on" : "off", build.c_str());
            if (!known) continue;
            That(leave.found == 1 && leave.store && leave.flag && leave.flag == leave.stored,
                 std::string("the masked exit anchor of ") + set.set->name + " matches once in build " + known->id + ", with its store at +0x47 writing the flag it tests");
            if (set.set->build != known) continue;
            const std::string json = scan::Body(scan::SdkRoot() / "data" / "builds" / known->id / "build.json");
            That(leave.anchor == set.set->guards[0].rva && leave.store == set.set->guards[1].rva && leave.flag == set.flag->at && leave.flag == set.tested->at,
                 std::string("the set ") + set.set->name + " names the anchor, store and flag found in the exe of its build");
            That(Value(json, "exit_anchor") == Hex(leave.anchor) && Value(json, "exit_store") == Hex(leave.store) && Value(json, "exit_flag") == Hex(leave.flag),
                 std::string("build.json of ") + known->id + " names the exit anchor, store and flag found in the exe");
            That(comes, std::string("every guard of ") + set.set->name + " matches the exe of its build, image size included");
        }
        if (known)
            That(on == 1 && armed && armed->set->build == known, std::string("on build ") + known->id + " exactly one exit set comes out on, the one pinned to it, so the exit watcher picks it");
        std::vector<const std::uint8_t *> seen;
        for (const nb::Pattern &p : nb::kPatterns)
        {
            if (std::find(seen.begin(), seen.end(), p.bytes) != seen.end()) continue;
            seen.push_back(p.bytes);
            const Near around = Neighbours(exe, p);
            std::printf("exescan:   row key %s anchor %u: %d windows one fixed byte away, %d two bytes away\n", Hex(p.keyRva).c_str(), p.anchorOffset, around.one, around.two);
            That(around.one == 0,"no other code window is one fixed byte away from the row of key " + Hex(p.keyRva) + " in " + one);
        }
    }
    return check::Finish("exescan");
}
