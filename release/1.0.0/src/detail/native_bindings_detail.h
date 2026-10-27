#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace zsdk::detail::native_bindings::detail
{
    enum class Kind : std::uint8_t { Code, Data };
    struct Pattern
    {
        std::uint32_t keyRva;
        const std::uint8_t *bytes;
        const std::uint8_t *mask;
        std::uint32_t size, anchorOffset, anchorSize;
        std::int32_t targetOffset;
        std::uint32_t displacementOffset, nextOffset;
        Kind kind;
    };
    struct Section
    {
        std::uint32_t rva, size;
        bool executable, readable;
        const std::uint8_t *bytes = nullptr;
    };
    struct Link
    {
        std::uint32_t patternIndex, displacementOffset, nextOffset, targetKey;
        std::int32_t addend;
    };
    struct Binding
    {
        std::uint32_t key, rva;
    };
    enum class Failure : std::uint8_t { None, Image, Catalog, Missing, Ambiguous, Range, Conflict, Link };
    struct Result
    {
        std::vector<Binding> bindings;
        Failure failure = Failure::None;
        std::uint32_t key = 0;
        std::size_t patterns = 0;
        std::size_t matched = 0;
    };

    inline bool Contains(std::size_t size, std::uint64_t at, std::uint64_t count)
    {
        return at <= size && count <= size - at;
    }

    inline bool Resolve(const std::uint8_t *image, std::size_t size, std::span<const Section> sections,
        std::span<const Pattern> patterns, Result &out, std::span<const Link> links = {})
    {
        out = {};
        const auto fail = [&](Failure why, std::uint32_t key) {
            out.bindings.clear();
            out.failure = why;
            out.key = key;
            return false;
        };
        if (!image || !size || size > std::numeric_limits<std::uint32_t>::max() || sections.empty() || patterns.empty())
            return fail(Failure::Image, 0);
        bool code = false;
        for (std::size_t i = 0; i < sections.size(); ++i)
        {
            const auto &section = sections[i];
            if (!section.size || !Contains(size, section.rva, section.size) || (section.executable && !section.readable))
                return fail(Failure::Image, 0);
            for (std::size_t j = 0; j < i; ++j)
                if (section.rva < std::uint64_t{sections[j].rva} + sections[j].size &&
                    sections[j].rva < std::uint64_t{section.rva} + section.size) return fail(Failure::Image, 0);
            code = code || section.executable;
        }
        if (!code || patterns.size() > 65536) return fail(Failure::Image, 0);
        std::vector<int> heads(65536, -1), next(patterns.size(), -1);
        std::vector<std::uint32_t> found(patterns.size());
        std::vector<const std::uint8_t *> matched(patterns.size());
        std::vector<std::uint32_t> starts(patterns.size());
        std::vector<bool> seen(patterns.size(), false);
        for (std::size_t i = 0; i < patterns.size(); ++i)
        {
            const auto &p = patterns[i];
            if (!p.keyRva || !p.bytes || !p.mask || !p.size || p.size > 65536 || p.anchorSize < 2 ||
                !Contains(p.size, p.anchorOffset, p.anchorSize) ||
                (p.kind != Kind::Code && p.kind != Kind::Data) ||
                (p.kind == Kind::Data && (!Contains(p.size, p.displacementOffset, 4) || p.nextOffset > p.size)))
                return fail(Failure::Catalog, p.keyRva);
            for (std::uint32_t j = 0; j < p.size; ++j)
                if (p.mask[j] != 0 && p.mask[j] != 0xFF) return fail(Failure::Catalog, p.keyRva);
            for (std::uint32_t j = 0; j < p.anchorSize; ++j)
                if (p.mask[p.anchorOffset + j] != 0xFF) return fail(Failure::Catalog, p.keyRva);
            if (p.kind == Kind::Data)
                for (std::uint32_t j = 0; j < 4; ++j)
                    if (p.mask[p.displacementOffset + j] != 0) return fail(Failure::Catalog, p.keyRva);
            std::uint16_t bucket = 0;
            std::memcpy(&bucket, p.bytes + p.anchorOffset, sizeof(bucket));
            next[i] = heads[bucket];
            heads[bucket] = static_cast<int>(i);
        }
        for (const auto &section : sections)
        {
            if (!section.executable || section.size < 2) continue;
            const auto *data = section.bytes ? section.bytes : image + section.rva;
            for (std::uint32_t at = 0; at <= section.size - 2; ++at)
            {
                std::uint16_t bucket = 0;
                std::memcpy(&bucket, data + at, sizeof(bucket));
                for (int index = heads[bucket]; index >= 0; index = next[index])
                {
                    const auto &p = patterns[index];
                    if (at < p.anchorOffset || !Contains(section.size, at, p.anchorSize) ||
                        !Contains(section.size, at - p.anchorOffset, p.size) ||
                        std::memcmp(data + at, p.bytes + p.anchorOffset, p.anchorSize)) continue;
                    const auto begin = at - p.anchorOffset;
                    bool equal = true;
                    for (std::uint32_t j = 0; j < p.size; ++j)
                        if (p.mask[j] && data[begin + j] != p.bytes[j]) { equal = false; break; }
                    if (!equal) continue;
                    if (seen[index]) return fail(Failure::Ambiguous, p.keyRva);
                    seen[index] = true;
                    matched[index] = data + begin;
                    starts[index] = section.rva + begin;
                    std::int64_t target = std::int64_t{section.rva} + begin + p.targetOffset;
                    if (p.kind == Kind::Data)
                    {
                        std::int32_t displacement = 0;
                        std::memcpy(&displacement, data + begin + p.displacementOffset, sizeof(displacement));
                        target += p.nextOffset + std::int64_t{displacement};
                    }
                    if (target <= 0 || target >= static_cast<std::int64_t>(size)) return fail(Failure::Range, p.keyRva);
                    const bool valid = std::any_of(sections.begin(), sections.end(), [&](const Section &s) {
                        return target >= s.rva && target < std::int64_t{s.rva} + s.size &&
                            (p.kind == Kind::Code ? s.executable : (s.readable && !s.executable));
                    });
                    if (!valid) return fail(Failure::Range, p.keyRva);
                    found[index] = static_cast<std::uint32_t>(target);
                }
            }
        }
        out.bindings.reserve(patterns.size());
        for (std::size_t i = 0; i < patterns.size(); ++i)
        {
            bool covered = false;
            for (std::size_t j = 0; j < patterns.size() && !covered; ++j)
                covered = seen[j] && patterns[j].keyRva == patterns[i].keyRva;
            if (!covered) return fail(Failure::Missing, patterns[i].keyRva);
            if (seen[i]) out.bindings.push_back({patterns[i].keyRva, found[i]});
        }
        std::sort(out.bindings.begin(), out.bindings.end(), [](const Binding &a, const Binding &b) { return a.key < b.key; });
        for (std::size_t i = 1; i < out.bindings.size(); ++i)
            if (out.bindings[i].key == out.bindings[i - 1].key && out.bindings[i].rva != out.bindings[i - 1].rva)
                return fail(Failure::Conflict, out.bindings[i].key);
        out.bindings.erase(std::unique(out.bindings.begin(), out.bindings.end(),
            [](const Binding &a, const Binding &b) { return a.key == b.key; }), out.bindings.end());
        for (const auto &link : links)
        {
            if (link.patternIndex >= patterns.size()) return fail(Failure::Catalog, link.targetKey);
            const auto &pattern = patterns[link.patternIndex];
            if (!Contains(pattern.size, link.displacementOffset, 4) || link.nextOffset > pattern.size)
                return fail(Failure::Catalog, pattern.keyRva);
            const auto operandMask = pattern.mask[link.displacementOffset];
            for (unsigned j = 1; j < 4; ++j)
                if (pattern.mask[link.displacementOffset + j] != operandMask) return fail(Failure::Catalog, pattern.keyRva);
            const auto target = std::lower_bound(out.bindings.begin(), out.bindings.end(), link.targetKey,
                [](const Binding &binding, std::uint32_t key) { return binding.key < key; });
            if (target == out.bindings.end() || target->key != link.targetKey) return fail(Failure::Catalog, link.targetKey);
            if (!seen[link.patternIndex]) continue;
            std::int32_t displacement = 0;
            std::memcpy(&displacement, matched[link.patternIndex] + link.displacementOffset, sizeof(displacement));
            const auto actual = std::int64_t{starts[link.patternIndex]} + link.nextOffset + displacement;
            const auto expected = std::int64_t{target->rva} + link.addend;
            if (actual != expected || actual <= 0 || actual >= static_cast<std::int64_t>(size)) return fail(Failure::Link, pattern.keyRva);
        }
        out.patterns = patterns.size();
        out.matched = static_cast<std::size_t>(std::count(seen.begin(), seen.end(), true));
        return true;
    }
}
