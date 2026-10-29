#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <regex>
#include <set>
#include <string>

namespace
{
    using check::That;

    constexpr int kLookCap = 5000;
    constexpr size_t kSkipLines = 8;

    struct Scan
    {
        std::filesystem::path workspace;
        std::filesystem::path self;
        bool chosen = false;
        std::vector<std::filesystem::path> trees;
        std::vector<std::string> skipped;
        int looked = 0;
        bool capped = false;
    };

    bool Has(const std::filesystem::path &path, bool folder)
    {
        std::error_code code;
        return folder ? std::filesystem::is_directory(path, code) : std::filesystem::is_regular_file(path, code);
    }

    bool Same(const std::filesystem::path &a, const std::filesystem::path &b)
    {
        std::error_code code;
        return a.lexically_normal() == b.lexically_normal() || std::filesystem::equivalent(a, b, code);
    }

    void Offer(Scan &s, const std::filesystem::path &root)
    {
        if (!Has(root / "src", true) || Same(root / "src", s.self)) return;
        if (s.chosen || Has(root / "build" / "zsdk.lock", false)) s.trees.push_back(root / "src");
        else s.skipped.push_back(scan::Generic(root.lexically_relative(s.workspace)) + " has a src folder but no build/zsdk.lock, so it is not a ZoneSDK add-on");
    }

    bool Look(Scan &s)
    {
        if (++s.looked <= kLookCap) return true;
        s.capped = true;
        return false;
    }

    void Walk(Scan &s)
    {
        for (const auto &one : scan::Dirs(s.workspace))
        {
            if (!Look(s)) return;
            Offer(s, one);
            Offer(s, one / "addon");
            for (const auto &two : scan::Dirs(one))
            {
                if (!Look(s)) return;
                if (two.filename() != "addon") Offer(s, two / "addon");
            }
        }
    }

    std::set<std::string> Tags(const std::filesystem::path &tree)
    {
        static const std::regex identity("Identity\\s*\\{\\s*\"[^\"]*\"\\s*,\\s*\"([A-Za-z0-9]+)\"");
        static const std::regex designated("\\.tag\\s*=\\s*\"([A-Za-z0-9]+)\"");
        static const std::regex wake("\"([A-Za-z0-9]+)\\.GameThread\\.1\"");
        static const std::regex mutex("Local\\\\\\\\([A-Za-z0-9]+)\\.Addon");
        std::set<std::string> out;
        for (const auto &file : scan::Files(tree, { ".h", ".cpp", ".inc" }, true))
        {
            const std::string body = scan::Body(file);
            for (const std::regex *one : { &identity, &designated, &wake, &mutex })
                for (std::sregex_iterator it(body.begin(), body.end(), *one), end; it != end; ++it) out.insert((*it)[1].str());
        }
        return out;
    }

    struct Peer
    {
        float lane = 0.0f;
        std::set<std::string> keys;
    };

    std::set<std::string> Chords(const std::string &list)
    {
        std::set<std::string> out;
        std::string one;
        for (const char c : list + "|")
        {
            if (c != '|')
            {
                if (c != ' ') one.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                continue;
            }
            if (!one.empty()) out.insert(one);
            one.clear();
        }
        return out;
    }

    std::vector<Peer> Peers(const std::filesystem::path &tree)
    {
        static const std::regex plain("Row\\s*\\{\\s*\"[a-z0-9_]+\"\\s*,\\s*\"[A-Za-z0-9]+\"\\s*,\\s*([0-9]*\\.?[0-9]+)f\\s*,\\s*\"([^\"]*)\"");
        static const std::regex named("\\{\\s*\\.id\\s*=\\s*\"[a-z0-9_]+\"\\s*,\\s*\\.tag\\s*=\\s*\"[A-Za-z0-9]+\"\\s*,\\s*\\.lane\\s*=\\s*([0-9]*\\.?[0-9]+)f\\s*,\\s*\\.keys\\s*=\\s*\"([^\"]*)\"");
        std::vector<Peer> out;
        for (const auto &file : scan::Files(tree, { ".h", ".cpp", ".inc" }, true))
        {
            const std::string body = scan::Body(file);
            for (const std::regex *one : { &plain, &named })
                for (std::sregex_iterator it(body.begin(), body.end(), *one), end; it != end; ++it) out.push_back(Peer{ std::stof((*it)[1].str()), Chords((*it)[2].str()) });
        }
        return out;
    }

    Scan Workspace(const addon::Tree &t)
    {
        Scan s;
        s.self = t.root / "src";
        const std::filesystem::path env = scan::EnvPath("ZSDK_WORKSPACE");
        const auto found = t.cfg.find("workspace");
        if (!env.empty())
        {
            s.workspace = env.lexically_normal();
            s.chosen = true;
        }
        else if (found != t.cfg.end() && !found->second.empty())
        {
            s.workspace = (t.root / found->second).lexically_normal();
            s.chosen = true;
        }
        else
        {
            std::filesystem::path root = t.root.lexically_normal();
            if (!root.has_filename()) root = root.parent_path();
            s.workspace = root.parent_path();
        }
        return s;
    }
}

int main()
{
    const addon::Tree t = addon::Load();
    const zsdk::Identity &id = zsdk::app::Get().identity;
    That(id.name && id.name[0] && id.tag && id.tag[0] && id.version && id.version[0], "the identity has a name, a tag and a version");
    That(std::string(id.name) == scan::Env("ZSDK_ADDON_NAME"), "the identity name is the built add-on");
    const zsdk::ini::Schema *schema = zsdk::app::Get().schema;
    That(schema && schema->visit && schema->make, "the App has a settings schema (App::schema); without one ZoneSDK opens no data folder, settings or log for the add-on and leaves it off");
    const auto registered = t.cfg.find("registry");
    if (registered != t.cfg.end()) That(registered->second == (id.registry ? id.registry : ""), "zsdk.cfg registry= names the registry id of the identity: " + registered->second + " against " + (id.registry ? id.registry : "none"));
    const auto defined = t.cfg.find("define");
    const std::string define = scan::Env("ZSDK_ADDON_DEFINE");
    if (defined != t.cfg.end() && !define.empty()) That(defined->second == define, "zsdk.cfg define= is the define the build uses: " + defined->second + " against " + define);
    bool plain = true;
    for (const char *walk = id.tag; walk && *walk; ++walk) plain = plain && std::isalnum(static_cast<unsigned char>(*walk));
    That(plain, "the tag is plain ASCII letters and digits");
    const zsdk::registry::Row *row = zsdk::app::Row();
    That(row != nullptr, std::string("the add-on has a registry row, its own or the ZoneSDK one: ") + (id.registry ? id.registry : "-"));
    if (row)
    {
        That(std::string(row->tag) == id.tag, "the registry row carries the same tag");
        That(!id.registry || std::string(row->id) == id.registry, "the registry row has the identity registry id");
        const zsdk::registry::Row *listed = zsdk::registry::Find(row->id);
        if (listed) That(std::string(listed->tag) == row->tag, "the ZoneSDK table agrees with the add-on row for " + std::string(row->id));
        else check::Warn(std::string("identity: ") + row->id + " is not in the ZoneSDK registry table, its own row is used");
        for (const auto &other : zsdk::registry::Rows())
        {
            if (std::string(other.id) == row->id) continue;
            That(std::string(other.tag) != row->tag, std::string("no other registry row uses the tag ") + row->tag + ": " + other.id);
            if (row->lane > 0.0f && other.lane == row->lane)
            {
                char lane[32]{};
                std::snprintf(lane, sizeof(lane), "%.2f", row->lane);
                check::Warn(std::string("identity: toast lane ") + lane + " is shared with " + other.id);
            }
        }
    }
    std::set<std::string> tags;
    for (const auto &r : zsdk::registry::Rows()) That(tags.insert(r.tag).second, std::string("registry tags are unique: ") + r.tag);
    const std::wstring wake = zsdk::WakeName(id);
    std::string wakeText;
    for (const wchar_t c : wake) wakeText.push_back(static_cast<char>(c & 0x7F));
    That(wakeText == std::string(id.tag) + ".GameThread.1", "the wake message derives from the tag");
    const std::wstring shape = zsdk::MutexShape(id);
    std::string shapeText;
    for (const wchar_t c : shape) shapeText.push_back(static_cast<char>(c & 0x7F));
    That(shapeText == std::string("Local\\") + id.tag + ".Addon.%lu", "the mutex derives from the tag");
    const std::set<std::string> same = addon::Allowed(t, "identity.same");
    Scan s = Workspace(t);
    That(Has(s.workspace, true), "the workspace for the collision scan exists: " + scan::Text(s.workspace));
    Walk(s);
    int scanned = 0;
    for (const auto &tree : s.trees)
    {
        ++scanned;
        const std::set<std::string> found = Tags(tree);
        const std::string owner = scan::Text(tree.parent_path().filename() == "addon" ? tree.parent_path().parent_path().filename() : tree.parent_path().filename());
        if (found.count(id.tag)) That(same.count(owner) == 1, std::string("another tree uses the tag ") + id.tag + " for its wake or mutex name: " + scan::Text(tree));
        if (!row || same.count(owner)) continue;
        const std::set<std::string> mine = Chords(row->keys ? row->keys : "");
        for (const Peer &peer : Peers(tree))
        {
            if (row->lane > 0.0f && peer.lane > 0.0f && std::fabs(peer.lane - row->lane) < 0.005f)
            {
                char lane[32]{};
                std::snprintf(lane, sizeof(lane), "%.2f", row->lane);
                check::Warn(std::string("identity: toast lane ") + lane + " is also used by the add-on in " + owner + "; its toasts and yours would overlap");
            }
            for (const std::string &key : mine)
                if (peer.keys.count(key)) check::Warn("identity: the key " + key + " is also a default key of the add-on in " + owner);
        }
    }
    for (size_t i = 0; i < s.skipped.size() && i < kSkipLines; ++i) std::printf("identity: skipped %s\n", s.skipped[i].c_str());
    if (s.skipped.size() > kSkipLines) std::printf("identity: skipped %d more folders without build/zsdk.lock\n", static_cast<int>(s.skipped.size() - kSkipLines));
    if (s.capped) check::Warn("identity: the collision scan stopped after " + std::to_string(kLookCap) + " folders under " + scan::Text(s.workspace) + "; point ZSDK_WORKSPACE or zsdk.cfg workspace= at the folder that holds your add-ons");
    std::printf("identity: %d sibling trees scanned under %s (%s), %d folders looked at, %d skipped\n", scanned, scan::Text(s.workspace).c_str(),
                s.chosen ? "chosen workspace" : "the add-on's parent folder, add-ons with build/zsdk.lock only", s.looked > kLookCap ? kLookCap : s.looked, static_cast<int>(s.skipped.size()));
    return check::Finish("zsdk-identity");
}
