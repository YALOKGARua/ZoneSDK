#pragma once

#include "version.h"

#include <zsdk/zsdk.h>

#include <cstdint>
#include <string>

namespace ztemplate
{
    namespace common = zsdk::verbs::common;

    inline constexpr const char *kVersion = TEMPLATE_VERSION;
    inline constexpr const char *kLocateKey = "Template.Locate";
    inline constexpr uint16_t kKey = 'L';

    struct Settings
    {
        std::string language = "auto";
        bool enabled = true;
        bool toasts = true;
        int logLevel = 1;
        zsdk::keys::Chord locate{ kKey, zsdk::keys::kAlt };
        friend bool operator==(const Settings &, const Settings &) = default;
    };

    inline constexpr zsdk::verbs::Spec kOwn[] = {
        { .owner = "Actor", .name = "K2_GetActorLocation", .required = true, .params = { { .name = "ReturnValue", .types = common::kStruct, .at = 0, .size = 24 } }, .mark = zsdk::verbs::Mark::Risky },
    };

    inline constexpr auto kSpecs = zsdk::verbs::Join(common::kWorld, kOwn);

    enum Verb : int { kLocation = common::kWorldCount, kVerbs };

    static_assert(kSpecs.size() == static_cast<size_t>(kVerbs));
    static_assert(zsdk::verbs::Counted(kSpecs, true) == 6 && zsdk::verbs::Counted(kSpecs, false) == 1);
    static_assert(zsdk::verbs::Index(kSpecs, "Actor", "K2_GetActorLocation") == kLocation && zsdk::verbs::Index(kSpecs, "GetFrameCount") == common::kFrameCount);

    enum class Order : int { Locate, Count };

    enum class Say : int { Located, Refused, Crashed, Count };

    struct Status
    {
        zsdk::Phase phase = zsdk::Phase::Off;
        bool coreReady = false;
        bool stable = false;
        std::string wait = "wait.world";
        std::string located;
        std::string refusal;
    };

    void Visit(Settings &s, zsdk::ini::Visitor &v);
    void Fix(Settings &s);
    const zsdk::ini::Schema &Schema();
    std::span<const zsdk::lang::Row> Rows();

    namespace core
    {
        void Refresh(bool again);
        void Prime();
        void Step(uint64_t now);
        bool Hot();
        void Publish(uint64_t now);
        void Turn(const zsdk::world::Turn &turn);
        void Order(int order);
        const char *OrderName(int order);
        bool Complete(const zsdk::census::Snapshot &snapshot);
        Status Snapshot();
        void Toast(Say say, const std::string &arg);
    }

    namespace frame
    {
        void Frame(const zsdk::Input &input);
    }
}
