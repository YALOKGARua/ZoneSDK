#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include "../src/detail/native_bindings_detail.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <set>
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

    constexpr std::uint32_t kCode = 0x1000;
    constexpr std::uint32_t kData = 0x100000;
    constexpr std::uint32_t kImage = 0x10000000;
    constexpr std::uint32_t kGap = 64;
    constexpr std::size_t kRows = std::size(nb::kPatterns);
    constexpr std::size_t kLinkRows = std::size(nb::kLinks);

    struct Image
    {
        std::vector<std::uint8_t> code;
        std::vector<std::uint32_t> at;
        std::vector<nb::Section> sections;
    };

    void Aim(Image &image, std::uint32_t at, std::uint32_t displacement, std::uint32_t next, std::int64_t wanted)
    {
        const auto value = static_cast<std::int32_t>(wanted - (std::int64_t{kCode} + at + next));
        std::memcpy(image.code.data() + at + displacement, &value, sizeof(value));
    }

    Image Make(std::uint8_t hole, bool twice, bool empty)
    {
        Image image;
        std::vector<const std::uint8_t *> placed;
        std::vector<std::uint32_t> where;
        image.at.resize(kRows);
        std::uint32_t cursor = kGap;
        for (std::size_t i = 0; i < kRows && !empty; ++i)
        {
            const auto &p = nb::kPatterns[i];
            const auto found = std::find(placed.begin(), placed.end(), p.bytes);
            if (found != placed.end())
            {
                image.at[i] = where[static_cast<std::size_t>(found - placed.begin())];
                continue;
            }
            placed.push_back(p.bytes);
            where.push_back(cursor);
            image.at[i] = cursor;
            cursor += p.size + kGap;
        }
        const std::uint32_t copy = cursor;
        if (twice) cursor += nb::kPatterns[0].size + kGap;
        image.code.assign(cursor, 0xCC);
        const auto lay = [&](const nb::Pattern &p, std::uint32_t at) {
            for (std::uint32_t j = 0; j < p.size; ++j) image.code[at + j] = p.mask[j] ? p.bytes[j] : hole;
        };
        for (std::size_t i = 0; i < kRows && !empty; ++i) lay(nb::kPatterns[i], image.at[i]);
        if (twice) lay(nb::kPatterns[0], copy);
        for (std::size_t i = 0; i < kRows && !empty; ++i)
        {
            const auto &p = nb::kPatterns[i];
            Aim(image, image.at[i], p.displacementOffset, p.nextOffset, std::int64_t{p.keyRva} - p.targetOffset);
            if (twice && p.bytes == nb::kPatterns[0].bytes) Aim(image, copy, p.displacementOffset, p.nextOffset, std::int64_t{p.keyRva} - p.targetOffset);
        }
        for (const auto &link : nb::kLinks)
            if (!empty) Aim(image, image.at[link.patternIndex], link.displacementOffset, link.nextOffset, std::int64_t{link.targetKey} + link.addend);
        image.sections.push_back({kCode, static_cast<std::uint32_t>(image.code.size()), true, true, image.code.data()});
        image.sections.push_back({kData, kImage - kData, false, true});
        return image;
    }

    bool Run(Image &image, nb::Result &out)
    {
        return nb::Resolve(image.code.data(), kImage, image.sections, nb::kPatterns, out, nb::kLinks);
    }
}

int main()
{
    std::set<std::uint32_t> keys;
    for (const auto &p : nb::kPatterns) keys.insert(p.keyRva);
    That(kRows >= 2 && keys.count(0xA0E68D0) && keys.count(0xA0E68E4) && keys.size() == 2, "the engine catalog binds the object root and the object count");
    std::uint32_t longest = 0;
    for (const auto &p : nb::kPatterns) longest = std::max(longest, p.size);
    That(longest >= 24 && longest <= 64, "every engine catalog pattern is a short masked window of 24 to 64 bytes, " + std::to_string(longest));
    for (std::size_t i = 0; i < kRows; ++i)
    {
        std::set<std::uint32_t> reached{ nb::kPatterns[i].keyRva };
        for (const auto &link : nb::kLinks)
            if (link.patternIndex == i) reached.insert(link.targetKey);
        That(reached == keys, "pattern row " + std::to_string(i) + " carries a RIP operand for every key, so a match is cross-checked");
    }

    for (const std::uint8_t hole : { std::uint8_t{0x00}, std::uint8_t{0xA5} })
    {
        Image image = Make(hole, false, false);
        nb::Result out;
        const bool ok = Run(image, out);
        const std::string tag = hole ? " (masked bytes 0xA5)" : " (masked bytes 0x00)";
        That(ok && out.failure == nb::Failure::None, "the catalog resolves in a synthetic image" + tag);
        That(out.patterns == kRows && out.matched == kRows, "every pattern row matches once" + tag);
        bool exact = out.bindings.size() == keys.size();
        for (const auto &binding : out.bindings) exact = exact && keys.count(binding.key) && binding.rva == binding.key;
        That(exact, "both keys come out at the wired addresses" + tag);
    }

    std::set<std::uint32_t> sharing;
    for (const auto &p : nb::kPatterns)
        if (p.bytes == nb::kPatterns[0].bytes) sharing.insert(p.keyRva);
    Image twice = Make(0x00, true, false);
    nb::Result doubled;
    That(!Run(twice, doubled) && doubled.failure == nb::Failure::Ambiguous && sharing.count(doubled.key), "a second copy of a window is refused as ambiguous");

    Image empty = Make(0x00, false, true);
    nb::Result none;
    That(!Run(empty, none) && none.failure == nb::Failure::Missing && none.bindings.empty(), "an image without the windows is refused as missing");

    std::size_t probe = kLinkRows;
    for (std::size_t i = 0; i < kLinkRows && probe == kLinkRows; ++i)
    {
        const auto &link = nb::kLinks[i];
        bool own = false;
        for (const auto &p : nb::kPatterns)
            own = own || (p.bytes == nb::kPatterns[link.patternIndex].bytes && p.displacementOffset == link.displacementOffset);
        if (!own) probe = i;
    }
    That(probe < kLinkRows, "the catalog has a cross-check operand that no row reads as its own");
    if (probe < kLinkRows)
    {
        Image bent = Make(0x00, false, false);
        const auto &link = nb::kLinks[probe];
        Aim(bent, bent.at[link.patternIndex], link.displacementOffset, link.nextOffset, std::int64_t{link.targetKey} + link.addend + 8);
        nb::Result broken;
        That(!Run(bent, broken) && broken.failure == nb::Failure::Link && broken.bindings.empty(), "a cross-check operand that points elsewhere is refused");
    }
    return check::Finish("fingerprints");
}
