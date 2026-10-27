#pragma once

#include <zsdk/ue.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zsdk::detail::engine
{
    using Param = ue::Param;
    using Sign = ue::Sign;
    using Pick = ue::Pick;

    constexpr uint64_t kParm = 0x80;
    constexpr uint64_t kOutParm = 0x100;
    constexpr uint64_t kReturnParm = 0x400;
    constexpr uint64_t kReferenceParm = 0x8000000;

    enum class Rode : int { Refused = -1, Faulted = 0, Sent = 1 };

    bool Bind();
    bool Wire();
    void Dusk();
    bool Ready();
    bool Bound();
    bool Dusked();
    bool Rejected();
    const char *Snag();

    bool Copy(const void *at, void *into, size_t bytes);
    bool Held(const void *spot, size_t bytes);
    const void *Grab(const void *thing, int32_t at);
    std::string Word(uint32_t slot);
    const uint8_t *Item(int32_t index);
    int32_t Total();
    int32_t Stride();
    const void *At(int32_t index);
    const void *ClassOf(const void *thing);
    const void *OuterOf(const void *thing);
    std::string Label(const void *thing);
    std::string Path(const void *thing);
    bool Kin(const void *type, const void *base);
    bool Inherits(const void *type, const char *name);
    bool Standing(const void *thing, const void *type);
    int32_t PropertyOffset(const void *type, const char *field);
    int32_t Bulk(const void *type);
    const void *FindClass(const char *className);
    const void *FindLive(const char *baseClass);
    const void *Host(const void *type);
    void ClearHosts();
    bool Analyze(const void *fn, Sign &out);
    bool Choices(const Param &one, std::vector<Pick> &out);

    void Survey();
    void Restock();
    bool Refresh();
    bool Booked();
    uint32_t BookEpoch();
    bool Stale();
    void Want();
    bool Wanted();
    bool Serve();
    std::vector<const void *> Exact(const std::string &name);

    Rode Ride(const void *verb, const void *thing, void *parms, void *fruit);
    const void *Facet(const void *thing, const void *verb);

#if defined(ZSDK_TESTS)
    void Pose(uint8_t **names, uint8_t *objects, uint8_t *count, int32_t width);
    void Poise();
#endif
}

namespace zsdk::detail::thread
{
    bool InPump();

    class Scope
    {
    public:
        Scope();
        ~Scope();
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
    };
}
