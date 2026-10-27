#pragma once

#include "version.h"

#include <zsdk/zsdk.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace zhellohud
{
    namespace common = zsdk::verbs::common;

    inline constexpr const char *kVersion = HELLOHUD_VERSION;
    inline constexpr uint16_t kKeyToggle = '7';

    enum class Corner : int { TopLeft, TopRight, BottomLeft, BottomRight, Count };
    enum class Unit : int { Kmh, Ms, Count };

    inline constexpr const char *kCorners[] = { "top_left", "top_right", "bottom_left", "bottom_right" };
    inline constexpr const char *kUnits[] = { "kmh", "ms" };

    static_assert(std::size(kCorners) == static_cast<size_t>(Corner::Count) && std::size(kUnits) == static_cast<size_t>(Unit::Count));

    struct Settings
    {
        std::string language = "auto";
        bool enabled = true;
        bool toasts = true;
        int logLevel = 1;
        bool shown = true;
        Corner corner = Corner::TopLeft;
        Unit unit = Unit::Kmh;
        zsdk::keys::Chord toggle{ kKeyToggle, zsdk::keys::kAlt };
        friend bool operator==(const Settings &, const Settings &) = default;
    };

    enum Verb : int { kLocation = common::kWorldCount, kVelocity, kRotation, kVerbs };

    inline constexpr zsdk::verbs::Spec kOwn[] = {
        { .owner = "Actor", .name = "K2_GetActorLocation", .required = true, .params = { { .name = "ReturnValue", .types = common::kStruct, .at = 0, .size = 24 } }, .mark = zsdk::verbs::Mark::Risky },
        { .owner = "Actor", .name = "GetVelocity", .required = false, .params = { { .name = "ReturnValue", .types = common::kStruct, .at = 0, .size = 24 } }, .mark = zsdk::verbs::Mark::Risky },
        { .owner = "Actor", .name = "K2_GetActorRotation", .required = false, .params = { { .name = "ReturnValue", .types = common::kStruct, .at = 0, .size = 24 } }, .mark = zsdk::verbs::Mark::Risky },
    };

    inline constexpr auto kSpecs = zsdk::verbs::Join(common::kWorld, kOwn);

    static_assert(kSpecs.size() == kVerbs);
    static_assert(zsdk::verbs::Index(kSpecs, "Actor", "K2_GetActorLocation") == kLocation && zsdk::verbs::Index(kSpecs, "Actor", "GetVelocity") == kVelocity &&
                  zsdk::verbs::Index(kSpecs, "Actor", "K2_GetActorRotation") == kRotation);
    static_assert(zsdk::verbs::Counted(kSpecs, true) == 6 && zsdk::verbs::Counted(kSpecs, false) == 3);

    enum class Order : int { Toggle, Count };

    enum class Say : int { Panel, Crashed, Count };

    struct Reading
    {
        zsdk::math::Vec3 at;
        zsdk::math::Vec3 velocity;
        zsdk::math::Rot3 rot;
        bool moving = false;
        bool turned = false;
        uint64_t when = 0;
    };

    struct Status
    {
        zsdk::Phase phase = zsdk::Phase::Off;
        bool coreReady = false;
        bool stable = false;
        bool live = false;
        std::string wait = "wait.world";
        std::string refusal;
        Reading reading;
        int reads = 0;
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
        std::string Section();
        void Toast(Say say, const std::string &arg);
    }

    namespace frame
    {
        void Frame(const zsdk::Input &input);
        bool Clear();
    }

    namespace view
    {
        inline constexpr uint64_t kReadMs = 100;
        inline constexpr uint64_t kStaleMs = 1000;

        struct Spot
        {
            float x = 0.0f;
            float y = 0.0f;
        };

        const char *PhaseId(zsdk::Phase phase);
        std::string State(const Status &st, zsdk::lang::Lang ui);
        double Bearing(double yaw);
        const char *Point(double bearing);
        double Pace(const zsdk::math::Vec3 &velocity, Unit unit);
        std::string Fixed(double value, int digits);
        bool Fresh(const Status &st, uint64_t now);
        std::vector<std::string> Lines(const Status &st, Unit unit, uint64_t now, zsdk::lang::Lang ui);
        Spot Place(Corner corner, float wide, float tall, float boxWide, float boxTall, float edge);
    }
}
