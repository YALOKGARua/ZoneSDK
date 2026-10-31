#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using check::That;

    struct Row
    {
        int size = -1;
        std::vector<zsdk::ue::Param> params;
        std::string source;
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
            r.source = parts[3];
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

    bool Fits(const Row &row, const zsdk::verbs::Spec &spec, std::string &why)
    {
        for (const zsdk::verbs::Want &want : spec.params)
        {
            if (!want.name) break;
            const zsdk::ue::Param *hit = nullptr;
            for (const zsdk::ue::Param &p : row.params)
                if (p.name == want.name) hit = &p;
            if (!hit)
            {
                why = std::string("no parameter ") + want.name;
                return false;
            }
            if (!zsdk::verbs::TypeFits(hit->type, want.types))
            {
                why = std::string("parameter ") + want.name + " is " + hit->type;
                return false;
            }
            if (want.at >= 0 && hit->at != want.at)
            {
                why = std::string("parameter ") + want.name + " at " + std::to_string(hit->at);
                return false;
            }
            if (want.size > 0 && hit->size > 0 && hit->size != want.size)
            {
                why = std::string("parameter ") + want.name + " size " + std::to_string(hit->size);
                return false;
            }
        }
        if (spec.count >= 0 && static_cast<int>(row.params.size()) != spec.count)
        {
            why = "parameter count " + std::to_string(row.params.size());
            return false;
        }
        if (spec.bytes >= 0 && row.size >= 0 && row.size != spec.bytes)
        {
            why = "block " + std::to_string(row.size);
            return false;
        }
        return true;
    }
}

int main()
{
    const addon::Tree t = addon::Load();
    const auto specs = zsdk::app::Get().specs;
    That(!specs.empty(), "the add-on has a verb table");
    namespace common = zsdk::verbs::common;
    const zsdk::WorldVerbs &world = zsdk::app::Get().world;
    const int roles[common::kWorldCount] = { world.controller, world.pawn, world.camera, world.seconds, world.paused, world.frames };
    for (int role = 0; role < common::kWorldCount; ++role)
    {
        const zsdk::verbs::Spec &want = common::kWorld[role];
        const int verb = roles[role];
        That(verb < 0 || common::Plays(specs, verb, static_cast<common::World>(role)),
             "WorldVerbs gives " + zsdk::verbs::KeyOf(want) + " the index of that verb in App::specs, or -1 for none (index " + std::to_string(verb) +
                 "); the defaults are the catalog indices, so a table that does not start with verbs::common::kWorld sets WorldVerbs itself");
    }
    const std::filesystem::path builds = t.sdk / "data" / "builds";
    int dumps = 0;
    for (const auto &folder : scan::Dirs(builds))
    {
        const std::string build = scan::Text(folder.filename());
        const std::filesystem::path file = folder / "ufunctions.txt";
        std::error_code code;
        if (!std::filesystem::is_regular_file(file, code))
        {
            std::printf("specs: build %s has no ufunctions.txt, nothing to check for it\n", build.c_str());
            continue;
        }
        ++dumps;
        const std::multimap<std::string, Row> dump = Dump(file);
        That(!dump.empty(), "the function dump of " + build + " is readable, " + std::to_string(dump.size()) + " rows");
        int verified = 0, missing = 0;
        std::string unlisted;
        for (const zsdk::verbs::Spec &spec : specs)
        {
            const auto range = dump.equal_range(spec.name);
            if (range.first == range.second)
            {
                ++missing;
                unlisted += (unlisted.empty() ? "" : ", ") + zsdk::verbs::KeyOf(spec);
                continue;
            }
            bool fits = false;
            std::string why;
            for (auto it = range.first; it != range.second && !fits; ++it) fits = Fits(it->second, spec, why);
            That(fits, "the dump of " + build + " agrees with " + zsdk::verbs::KeyOf(spec) + (fits ? std::string() : " (" + why + ")"));
            if (fits) ++verified;
        }
        std::printf("specs: build %s, %d of %d verbs checked against the dump, %d not in the dump: %s\n", build.c_str(), verified, static_cast<int>(specs.size()), missing,
                    unlisted.empty() ? "none" : unlisted.c_str());
    }
    if (dumps == 0)
        check::Skip("zsdk-specs: the ZoneSDK release has no function dump (data/builds/<build>/ufunctions.txt), the verb table is checked only against live reflection when the add-on binds in game");
    return check::Finish("zsdk-specs");
}
