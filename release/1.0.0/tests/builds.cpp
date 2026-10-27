#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/quit.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <string>

namespace
{
    using check::That;

    std::string Value(const std::string &json, const std::string &key)
    {
        const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch hit;
        return std::regex_search(json, hit, pattern) ? hit[1].str() : std::string();
    }

    std::string Hex(uint32_t value)
    {
        char text[16]{};
        std::snprintf(text, sizeof(text), "0x%X", value);
        return text;
    }
}

int main()
{
    namespace quit = zsdk::detail::quit;
    const std::filesystem::path sdk = scan::SdkRoot();
    int exits = 0;
    for (const zsdk::guard::Build *build : zsdk::guard::kBuilds)
    {
        const std::filesystem::path folder = sdk / "data" / "builds" / build->id;
        const std::string json = scan::Body(folder / "build.json");
        That(!json.empty(), std::string("build.json exists for ") + build->id);
        That(Value(json, "id") == build->id && Value(json, "sha256") == build->sha256, "id and sha agree");
        That(Value(json, "image_size") == Hex(build->imageSize) && Value(json, "stamp") == Hex(build->stamp), "image size and stamp agree");
        That(Value(json, "objects_root") == Hex(build->objectsRoot) && Value(json, "objects_count") == Hex(build->objectsCount), "the object table keys agree");
        std::string prefix = std::string(build->sha256).substr(0, 8);
        for (char &c : prefix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        That(prefix == build->id, std::string("the id is the first 8 hex digits of the sha256 in lower case: ") + build->id);
        That(std::regex_match(Value(json, "file_size"), std::regex("[1-9][0-9]*")), std::string("build.json gives the exe file size of ") + build->id);
        const uint32_t anchor = static_cast<uint32_t>(std::strtoul(Value(json, "exit_anchor").c_str(), nullptr, 16));
        const uint32_t store = static_cast<uint32_t>(std::strtoul(Value(json, "exit_store").c_str(), nullptr, 16));
        const uint32_t flag = static_cast<uint32_t>(std::strtoul(Value(json, "exit_flag").c_str(), nullptr, 16));
        That(anchor && Value(json, "exit_anchor") == Hex(anchor) && Value(json, "exit_store") == Hex(store) && Value(json, "exit_flag") == Hex(flag) && store == anchor + 0x47 && flag > store && flag < build->imageSize,
             std::string("build.json gives RequestEngineExit, its store at +0x47 and GIsRequestingExit inside the image of ") + build->id);
        int mine = 0;
        for (const quit::Exit &one : quit::kExits)
        {
            if (one.set->build != build) continue;
            ++mine;
            That(anchor == one.set->guards[0].rva && store == one.set->guards[1].rva && flag == one.flag->at && flag == one.tested->at,
                 std::string("the exit anchor, store and flag of ") + build->id + " in build.json are those of the set " + one.set->name);
        }
        That(mine == 1, std::string("one zsdk.exit set is pinned to build ") + build->id);
        exits += mine;
        That(!std::regex_search(json, std::regex("question\\s+\\d", std::regex::icase)), "build.json carries no internal planning wording");
        That(std::filesystem::exists(folder / "proven.txt"), "the proven list exists");
        That(std::filesystem::exists(folder / "ufunctions.txt"), "the function dump exists");
    }
    That(exits == static_cast<int>(std::size(quit::kExits)) && std::size(quit::kExits) == std::size(zsdk::guard::kBuilds), "one exit set per known build, and every exit set is pinned to a known build");
    size_t rows = 0;
    const std::regex key("^[A-Za-z0-9_]+(\\.[A-Za-z0-9_]+)+$");
    for (const zsdk::guard::Build *build : zsdk::guard::kBuilds)
    {
        std::istringstream lines(scan::Body(sdk / "data" / "builds" / build->id / "proven.txt"));
        std::string line;
        while (std::getline(lines, line))
        {
            const std::string one = scan::Trim(line);
            if (one.empty()) continue;
            ++rows;
            That(std::regex_match(one, key), std::string("proven.txt row is a key: ") + one);
            That(zsdk::ledger::Preloaded(one, build->id), std::string("proven.txt row is compiled into the preload table: ") + one);
        }
    }
    That(zsdk::ledger::PreloadCount() == rows, "the preload table has exactly the proven.txt rows, " + std::to_string(rows));
    const std::string catalog = scan::Body(sdk / "src" / "detail" / "native_bindings_catalog.inc");
    That(catalog.find("0xA0E68D0") != std::string::npos || catalog.find("0xa0e68d0") != std::string::npos, "the engine catalog binds the object root");
    return check::Finish("builds");
}
