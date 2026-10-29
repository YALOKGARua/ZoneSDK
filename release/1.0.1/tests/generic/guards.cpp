#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <windows.h>

#include <bcrypt.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace
{
    using check::That;

    struct Section
    {
        uint32_t va = 0;
        uint32_t size = 0;
        uint32_t raw = 0;
        uint32_t rawSize = 0;
    };

    class Image
    {
    public:
        Image() = default;
        Image(const Image &) = delete;
        Image &operator=(const Image &) = delete;

        ~Image()
        {
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        }

        bool Open(const std::filesystem::path &path, std::string &why)
        {
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                why = "it does not open for reading, error " + std::to_string(GetLastError());
                return false;
            }
            LARGE_INTEGER size{};
            if (!GetFileSizeEx(file, &size))
            {
                why = "its size is unknown";
                return false;
            }
            length = static_cast<uint64_t>(size.QuadPart);
            IMAGE_DOS_HEADER dos{};
            if (!Read(0, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
            {
                why = "it is not a PE file";
                return false;
            }
            IMAGE_NT_HEADERS64 nt{};
            if (!Read(static_cast<uint64_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                why = "it is not a 64-bit PE image";
                return false;
            }
            imageSize = nt.OptionalHeader.SizeOfImage;
            stamp = nt.FileHeader.TimeDateStamp;
            uint64_t at = static_cast<uint64_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i, at += sizeof(IMAGE_SECTION_HEADER))
            {
                IMAGE_SECTION_HEADER h{};
                if (!Read(at, &h, sizeof(h)))
                {
                    why = "its section table is cut short";
                    return false;
                }
                sections.push_back(Section{ h.VirtualAddress, h.Misc.VirtualSize, h.PointerToRawData, h.SizeOfRawData });
            }
            if (sections.empty())
            {
                why = "it has no sections";
                return false;
            }
            sha = Hash();
            if (sha.empty())
            {
                why = "its SHA-256 could not be computed";
                return false;
            }
            return true;
        }

        bool Bytes(uint32_t rva, uint8_t *out, size_t n) const
        {
            for (const Section &s : sections)
                if (rva >= s.va && rva - s.va < std::max(s.size, s.rawSize))
                    return static_cast<uint64_t>(rva - s.va) + n <= s.rawSize && Read(static_cast<uint64_t>(s.raw) + (rva - s.va), out, n);
            return false;
        }

        uint64_t length = 0;
        uint32_t imageSize = 0;
        uint32_t stamp = 0;
        std::string sha;

    private:
        bool Read(uint64_t at, void *out, size_t n) const
        {
            OVERLAPPED where{};
            where.Offset = static_cast<DWORD>(at & 0xFFFFFFFFull);
            where.OffsetHigh = static_cast<DWORD>(at >> 32);
            DWORD got = 0;
            return ReadFile(file, out, static_cast<DWORD>(n), &got, &where) && got == n;
        }

        std::string Hash() const
        {
            BCRYPT_ALG_HANDLE alg = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            std::string out;
            if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) && BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0)))
            {
                std::vector<uint8_t> chunk(4u << 20);
                bool whole = true;
                for (uint64_t at = 0; at < length && whole; at += chunk.size())
                {
                    const size_t n = static_cast<size_t>(std::min<uint64_t>(chunk.size(), length - at));
                    whole = Read(at, chunk.data(), n) && BCRYPT_SUCCESS(BCryptHashData(hash, chunk.data(), static_cast<ULONG>(n), 0));
                }
                uint8_t digest[32]{};
                if (whole && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0)))
                {
                    char hex[65]{};
                    for (int i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02X", digest[i]);
                    out = hex;
                }
            }
            if (hash) BCryptDestroyHash(hash);
            if (alg) BCryptCloseAlgorithmProvider(alg, 0);
            return out;
        }

        HANDLE file = INVALID_HANDLE_VALUE;
        std::vector<Section> sections;
    };

    std::string Hex(uint32_t value)
    {
        char text[16]{};
        std::snprintf(text, sizeof(text), "0x%X", value);
        return text;
    }

    const char *NeedText(zsdk::guard::Need need)
    {
        switch (need)
        {
        case zsdk::guard::Need::ImageSize: return "bytes and image size";
        case zsdk::guard::Need::KnownBuild: return "bytes, image size and the exact build";
        default: return "bytes";
        }
    }

    bool Matches(const Image &img, const zsdk::guard::Guard &g, std::string &why)
    {
        std::vector<int> want;
        if (!zsdk::guard::Hex(g.bytes, want))
        {
            why = "bad hex";
            return false;
        }
        std::vector<uint8_t> got(want.size());
        if (!img.Bytes(g.rva, got.data(), got.size()))
        {
            why = "outside the file";
            return false;
        }
        for (size_t i = 0; i < want.size(); ++i)
            if (want[i] >= 0 && got[i] != static_cast<uint8_t>(want[i]))
            {
                char text[48]{};
                std::snprintf(text, sizeof(text), "byte %zu is %02X, wanted %02X", i, got[i], want[i]);
                why = text;
                return false;
            }
        return true;
    }
}

int main()
{
    const auto listed = zsdk::app::Get().sets;
    const auto own = zsdk::guard::Own();
    std::vector<const zsdk::guard::Set *> sets(own.begin(), own.end());
    sets.insert(sets.end(), listed.begin(), listed.end());
    const bool require = scan::Env("ZSDK_REQUIRE_EXE") == "1";
    std::set<std::string> names;
    int guards = 0;
    for (const zsdk::guard::Set *set : own) That(set && set->name && std::string(set->name).rfind("zsdk.", 0) == 0, "the SDK's own guard sets are named zsdk.*");
    for (const zsdk::guard::Set *set : listed) That(!set || !set->name || std::string(set->name).rfind("zsdk.", 0) != 0, "an add-on guard set does not take the zsdk.* names of the SDK's own sets");
    for (const zsdk::guard::Set *set : sets)
    {
        That(set && set->name && set->name[0], "every guard set has a name");
        if (!set || !set->name) continue;
        const std::string name = set->name;
        That(names.insert(name).second, "guard set names are unique: " + name);
        That(set->count == 0 || set->guards, "the guard set " + name + " points at its guards");
        That(set->need == zsdk::guard::Need::Bytes || set->build, "the guard set " + name + " names the build its image size comes from");
        for (size_t i = 0; set->guards && i < set->count; ++i)
        {
            const zsdk::guard::Guard &g = set->guards[i];
            std::vector<int> want;
            const bool parsed = zsdk::guard::Hex(g.bytes, want);
            That(parsed, "the guard bytes parse: " + name + " " + Hex(g.rva));
            if (parsed && set->build) That(static_cast<uint64_t>(g.rva) + want.size() <= set->build->imageSize, "the guard lies inside the image of " + std::string(set->build->id) + ": " + name + " " + Hex(g.rva));
            ++guards;
        }
    }
    const std::vector<addon::Declared> declared = addon::GuardSets(addon::Load());
    int unlisted = 0;
    for (const addon::Declared &d : declared)
    {
        if (!d.name.empty() && names.count(d.name)) continue;
        ++unlisted;
        const std::string what = d.name.empty() ? "the guard set " + d.id + " in " + d.file + " has no literal name, so this suite cannot tell whether App.sets lists it"
                                                : "the guard set " + d.name + " (" + d.id + " in " + d.file + ") is not listed in App.sets, so neither the SDK boot check nor this suite compares it with a game exe";
        if (require) That(false, what + "; under -RequireExe every declared set is listed in .sets");
        else check::Warn("guards: " + what + "; list it in .sets");
    }
    if (listed.empty())
    {
        if (declared.empty()) std::printf("guards: the add-on declares no guard sets, so only ZoneSDK's own guard sets are compared: %d\n", static_cast<int>(own.size()));
        else std::printf("guards: the add-on declares %d guard sets in its sources and lists none in App.sets, %d unlisted\n", static_cast<int>(declared.size()), unlisted);
    }
    else if (unlisted > 0) std::printf("guards: %d of the %d guard sets declared in the sources are not listed in App.sets\n", unlisted, static_cast<int>(declared.size()));
    if (sets.empty()) return check::Finish("zsdk-guards");
    const std::filesystem::path path = scan::EnvPath("ZSDK_EXE");
    const std::string tail = std::to_string(guards) + " guards in " + std::to_string(sets.size()) + " sets were not compared with a game exe";
    if (path.empty())
    {
        if (require) That(false, "an exe is given for the guard check under -RequireExe (ZSDK_EXE, make -Exe or -LiveExe)");
        else check::Skip("zsdk-guards: no exe given (ZSDK_EXE, make -Exe or -LiveExe), " + tail);
        return check::Finish("zsdk-guards");
    }
    Image img;
    std::string why;
    if (!img.Open(path, why))
    {
        if (require) That(false, "the exe " + scan::Text(path.filename()) + " opens read-only as a PE image under -RequireExe (" + why + ")");
        else check::Skip("zsdk-guards: the exe " + scan::Text(path.filename()) + " cannot be used (" + why + "), " + tail);
        return check::Finish("zsdk-guards");
    }
    const zsdk::guard::Build *build = zsdk::guard::Find(img.sha);
    const zsdk::guard::Build *exeBuild = build;
    for (const zsdk::guard::Set *set : sets)
        if (!exeBuild && set && set->build && img.sha == set->build->sha256) exeBuild = set->build;
    std::printf("guards: exe %s, %llu bytes, sha256 %s..., image %s, stamp %s, %s\n", scan::Text(path.filename()).c_str(), static_cast<unsigned long long>(img.length), img.sha.substr(0, 16).c_str(),
                Hex(img.imageSize).c_str(), Hex(img.stamp).c_str(), build ? ("build " + std::string(build->id) + " known to this ZoneSDK").c_str() : "a build this ZoneSDK does not know, forecast only");
    int on = 0, off = 0, compared = 0, elsewhere = 0;
    for (const zsdk::guard::Set *set : sets)
    {
        if (!set || !set->name) continue;
        const std::string name = set->name;
        int ok = 0;
        std::string bad;
        for (size_t i = 0; set->guards && i < set->count; ++i)
        {
            std::string one;
            if (Matches(img, set->guards[i], one)) ++ok;
            else bad += (bad.empty() ? "" : "; ") + Hex(set->guards[i].rva) + " " + one;
            ++compared;
        }
        const int total = static_cast<int>(set->count);
        const bool imageOk = set->build && img.imageSize == set->build->imageSize;
        const bool known = set->build && img.sha == set->build->sha256;
        bool fine = ok == total;
        if (set->need == zsdk::guard::Need::ImageSize || set->need == zsdk::guard::Need::KnownBuild) fine = fine && imageOk;
        if (set->need == zsdk::guard::Need::KnownBuild) fine = fine && known;
        const bool other = !fine && exeBuild && set->build && !known && set->need != zsdk::guard::Need::Bytes;
        if (fine) ++on;
        else ++off;
        if (other) ++elsewhere;
        std::string reason;
        if (!fine)
        {
            if (ok != total) reason = ", differs at " + bad;
            else if (set->need != zsdk::guard::Need::Bytes && !imageOk) reason = ", the image size is " + Hex(img.imageSize) + (set->build ? ", the set wants " + Hex(set->build->imageSize) : std::string());
            else reason = ", the exe is not build " + std::string(set->build ? set->build->id : "-");
            if (other) reason += "; the set is for build " + std::string(set->build->id) + " and the exe is build " + exeBuild->id;
        }
        std::printf("guards: set %s (needs %s, build %s): %d/%d guards match, image %s -> %s%s\n", name.c_str(), NeedText(set->need), set->build ? set->build->id : "-", ok, total,
                    imageOk ? "ok" : "differs", fine ? "on" : "off", reason.c_str());
        if (known) That(ok == total && imageOk, "the exe is build " + std::string(set->build->id) + " of the set " + name + " by SHA-256, so every guard and the image size match" + reason);
        else if (require && !other) That(fine, "the set " + name + " comes out on under -RequireExe" + reason);
    }
    const std::string spare = elsewhere ? ", " + std::to_string(elsewhere) + " of them for another build" : std::string();
    std::printf("guards: %d guards in %d sets compared with the exe, forecast %d on, %d off%s%s\n", compared, static_cast<int>(sets.size()), on, off, spare.c_str(),
                require ? "; under -RequireExe every set must come out on except a set pinned to another build" : build ? "" : "; on an unknown build this is a forecast, it never fails the build");
    return check::Finish("zsdk-guards");
}
