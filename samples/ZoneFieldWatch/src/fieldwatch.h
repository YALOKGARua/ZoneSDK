#pragma once

#include "fields.h"
#include "version.h"

#include <zsdk/zsdk.h>

#include <cstdint>
#include <span>
#include <string>

namespace zfieldwatch
{
    inline constexpr const char *kVersion = FIELDWATCH_VERSION;
    inline constexpr const char *kDefaultKey = "FieldWatch.DefaultFov";
    inline constexpr const char *kLockedKey = "FieldWatch.LockedFov";
    inline constexpr const char *kJudgeJob = "fieldwatch.verdict";
    inline constexpr uint32_t kReadMs = 250;
    inline constexpr uint32_t kJudgeMs = 500;

    struct Settings
    {
        std::string language = "auto";
        bool enabled = true;
        int logLevel = 1;
        friend bool operator==(const Settings &, const Settings &) = default;
    };

    enum class Verdict : uint8_t { Pending, Waiting, Read, Off, Crashed, Unmarked, Unproven };

    enum class Build : uint8_t { Unchecked, Known, Other, Rejected };

    struct Reading
    {
        Verdict verdict = Verdict::Waiting;
        float value = 0.0f;
        friend bool operator==(const Reading &, const Reading &) = default;
    };

    struct Watched
    {
        Reading reading;
        bool proven = false;
        friend bool operator==(const Watched &, const Watched &) = default;
    };

    struct Board
    {
        Build build = Build::Unchecked;
        std::string id;
        const zsdk::guard::Set *camera = nullptr;
        zsdk::guard::State set = zsdk::guard::State::Pending;
        Verdict fov = Verdict::Pending;
        Verdict locked = Verdict::Pending;
        Verdict view = Verdict::Unproven;
        friend bool operator==(const Board &, const Board &) = default;
    };

    struct Status
    {
        zsdk::Phase phase = zsdk::Phase::Off;
        bool stable = false;
        bool enabled = true;
        std::string wait = "wait.world";
        Board board;
        Watched fov;
        Watched locked;
    };

    void Visit(Settings &s, zsdk::ini::Visitor &v);
    void Fix(Settings &s);
    const zsdk::ini::Schema &Schema();
    std::span<const zsdk::lang::Row> Rows();

    namespace watch
    {
        void Checked(bool done);
        const zsdk::guard::Field &On(std::span<const zsdk::guard::Field *const> fields);
        const zsdk::guard::Field &Locked();
        Verdict Gate(const zsdk::guard::Field &field);
        Reading Read(const void *object, const zsdk::guard::Field &field, const char *key);
        Watched Live(const zsdk::guard::Field &field, const char *key, const Reading &last);
        Board Look();
        const char *Word(Verdict verdict);
    }

    namespace core
    {
        void Boot();
        void Refresh(bool again);
        void Prime();
        void Step(uint64_t now);
        bool Hot();
        void Publish(uint64_t now);
        void Turn(const zsdk::world::Turn &turn);
        bool Complete(const zsdk::census::Snapshot &snapshot);
        void Judge(uint64_t now);
        Status Snapshot();
    }
}
