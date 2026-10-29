#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using check::That;
    namespace verbs = zsdk::verbs;
    namespace common = zsdk::verbs::common;

    struct Row
    {
        int size = -1;
        std::vector<zsdk::ue::Param> params;
    };

    std::multimap<std::string, Row> Dump(const std::filesystem::path &file)
    {
        std::multimap<std::string, Row> out;
        std::istringstream lines(scan::Body(file));
        std::string line;
        while (std::getline(lines, line))
        {
            const std::vector<std::string> parts = zsdk::text::Split(line, '|');
            if (parts.size() < 4) continue;
            Row r;
            r.size = std::atoi(parts[1].c_str());
            for (const std::string &one : zsdk::text::Split(parts[2], ';'))
            {
                const std::vector<std::string> f = zsdk::text::Split(one, ':');
                if (f.size() < 4) continue;
                zsdk::ue::Param p;
                p.name = f[0];
                p.type = f[1];
                p.at = std::atoi(f[2].c_str());
                p.size = std::atoi(f[3].c_str());
                r.params.push_back(p);
            }
            out.emplace(parts[0], r);
        }
        return out;
    }

    bool Fits(const Row &row, const verbs::Spec &spec)
    {
        for (const verbs::Want &want : spec.params)
        {
            if (!want.name) break;
            const zsdk::ue::Param *hit = nullptr;
            for (const zsdk::ue::Param &p : row.params)
                if (p.name == want.name) hit = &p;
            if (!hit || !verbs::TypeFits(hit->type, want.types) || (want.at >= 0 && hit->at != want.at) || (want.size > 0 && hit->size > 0 && hit->size != want.size)) return false;
        }
        return true;
    }

    constexpr verbs::Spec kOwn[] = {
        { "Actor", "K2_GetActorLocation", true, { { "ReturnValue", common::kStruct, 0, 24 } }, verbs::Mark::Risky },
    };

    constexpr int kWorlds = common::kWorldCount;
    constexpr int kConsoles = common::kConsoleCount;
    constexpr auto kJoined = verbs::Join(verbs::Join(common::kWorld, common::kConsole), kOwn);
    static_assert(kJoined.size() == static_cast<size_t>(kWorlds + kConsoles + 1));
    static_assert(verbs::Index(kJoined, "GetPlayerController") == common::kPlayerController && verbs::Index(kJoined, "GetFrameCount") == common::kFrameCount);
    static_assert(verbs::Index(kJoined, "XNeedsSetNeedsValue") == kWorlds + common::kNeedsValue && verbs::Index(kJoined, "K2_GetActorLocation") == kWorlds + kConsoles);
    static_assert(verbs::Index(kJoined, "CustomConsoleManagerAI2", "XDestroyModel") == kWorlds + common::kDestroyModel && verbs::Index(kJoined, "Actor", "XDestroyModel") == -1);
    static_assert(verbs::Index(kJoined, "Nothing") == -1 && verbs::Counted(kJoined, true) == 6 && verbs::Counted(kJoined, false) == 3);
}

int main()
{
    const zsdk::WorldVerbs world;
    That(world.controller == verbs::Index(common::kWorld, "GetPlayerController") && world.pawn == verbs::Index(common::kWorld, "GetPlayerPawn") &&
             world.camera == verbs::Index(common::kWorld, "GetPlayerCameraManager") && world.seconds == verbs::Index(common::kWorld, "GetTimeSeconds") &&
             world.paused == verbs::Index(common::kWorld, "IsGamePaused") && world.frames == verbs::Index(common::kWorld, "GetFrameCount"),
         "WorldVerbs defaults to the common world rows");
    That(kJoined[kWorlds].mark == verbs::Mark::Risky && kJoined[kJoined.size() - 1].mark == verbs::Mark::Risky && kJoined[0].mark == verbs::Mark::Auto, "joined rows keep their marks");
    std::set<std::string> keys;
    std::vector<const verbs::Spec *> rows;
    for (const verbs::Spec &spec : common::kWorld) rows.push_back(&spec);
    for (const verbs::Spec &spec : common::kScreen) rows.push_back(&spec);
    for (const verbs::Spec &spec : common::kConsole) rows.push_back(&spec);
    for (const verbs::Spec *spec : rows)
    {
        That(keys.insert(verbs::KeyOf(*spec)).second, "catalog keys are unique: " + verbs::KeyOf(*spec));
        That(spec->params[0].name != nullptr, "every catalog row names its parameters: " + verbs::KeyOf(*spec));
    }
    for (const verbs::Spec &spec : common::kScreen) That(!spec.required && spec.mark == verbs::Mark::Auto, "screen checks are optional reads: " + verbs::KeyOf(spec));
    for (const verbs::Spec &spec : common::kConsole) That(!spec.required && spec.mark == verbs::Mark::Risky && std::string(spec.owner) == common::kConsoleHost, "console commands are optional, risky and on one host: " + verbs::KeyOf(spec));
    const std::filesystem::path builds = scan::SdkRoot() / "data" / "builds";
    int dumps = 0;
    for (const auto &folder : scan::Dirs(builds))
    {
        const std::filesystem::path file = folder / "ufunctions.txt";
        std::error_code code;
        if (!std::filesystem::is_regular_file(file, code)) continue;
        ++dumps;
        const std::multimap<std::string, Row> dump = Dump(file);
        int checked = 0;
        std::string unlisted;
        for (const verbs::Spec *spec : rows)
        {
            const auto range = dump.equal_range(spec->name);
            if (range.first == range.second)
            {
                unlisted += (unlisted.empty() ? "" : ", ") + verbs::KeyOf(*spec);
                continue;
            }
            bool fits = false;
            for (auto it = range.first; it != range.second && !fits; ++it) fits = Fits(it->second, *spec);
            That(fits, "the dump of " + scan::Text(folder.filename()) + " agrees with the catalog row " + verbs::KeyOf(*spec));
            ++checked;
        }
        That(checked >= 5, "the dump covers the controller, pawn, frame and console rows, " + std::to_string(checked) + " checked");
        std::printf("catalog: build %s, %d of %d rows checked against the dump, not in the dump: %s\n", scan::Text(folder.filename()).c_str(), checked, static_cast<int>(rows.size()),
                    unlisted.empty() ? "none" : unlisted.c_str());
    }
    That(dumps > 0, "the SDK tree has a function dump");
    return check::Finish("catalog");
}
