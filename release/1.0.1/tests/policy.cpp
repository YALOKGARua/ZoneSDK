#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include "support/probe.h"

#include <regex>
#include <set>
#include <string>

int main()
{
    using check::That;
    zsdk::Policy d;
    That(!zsdk::Legacy(d) && zsdk::LegacyText(d) == "none", "the defaults are not legacy");
    zsdk::Policy g;
    g.marks = zsdk::Marks::Flagged;
    g.haltOnSeh = false;
    g.gone = zsdk::GoneFlags::Legacy;
    g.spelling = zsdk::Spelling::Strict;
    g.widen = true;
    g.knock = zsdk::Knock::Configured;
    g.knockMs = 33;
    g.wire = zsdk::WireRetry::Fixed;
    That(zsdk::Legacy(g), "a changed field is legacy");
    That(zsdk::LegacyText(g) == "marks=flagged, haltOnSeh=0, gone=legacy, spelling=strict, widen=1, knock=configured 33/1000, wire=fixed 2000", "the legacy line names every changed field");
    zsdk::Policy s;
    s.verifiedOnly = false;
    s.strictMarks = false;
    That(zsdk::Legacy(s) && zsdk::LegacyText(s) == "strictMarks=0, verifiedOnly=0", "the call checks of 0.1.1 are legacy fields too");
    That(d.verifiedOnly && d.strictMarks, "the defaults refuse unverified verbs and unwritten crash markers");
    zsdk::Policy m;
    m.mods = zsdk::ModRule::Exact;
    That(zsdk::Legacy(m) && zsdk::LegacyText(m) == "mods=exact", "the mod-key rule is a policy field");
    std::set<std::string> tags;
    std::set<std::string> ids;
    for (const auto &row : zsdk::registry::Rows())
    {
        That(tags.insert(row.tag).second, std::string("registry tags are unique: ") + row.tag);
        That(ids.insert(row.id).second, std::string("registry ids are unique: ") + row.id);
        That(row.lane >= 0.0f && row.lane < 1.0f, std::string("lanes are on screen: ") + row.id);
    }
    That(zsdk::registry::Find("grenades") && zsdk::registry::Lane("grenades", 0) == 0.48f, "ZoneGrenades keeps its toast row 0.48");
    That(zsdk::registry::Lane("spawner", 0.5f) == 0.5f && zsdk::registry::Find("nobody") == nullptr, "a table row with lane 0 and a missing row fall back in registry::Lane");
    That(zsdk::registry::Find("spawner") != nullptr, "the Spawner has a registry row");
    const std::set<std::string> released = { "ride", "heli", "balloon", "mount", "skif", "zipline", "grenades", "underground", "limansk", "mercy", "spawner", "pulse", "coop", "newland", "cheats", "blinddog", "wounds", "template" };
    std::set<std::string> listed;
    for (const auto &row : zsdk::registry::Rows()) listed.insert(row.id);
    That(listed == released, "the table lists the add-ons released on Nexus and the template, nothing else, " + std::to_string(listed.size()) + " rows");
    for (const char *never : { "terra", "imgui", "livemap" }) That(zsdk::registry::Find(never) == nullptr, std::string("no row for an add-on that is not released: ") + never);
    for (const auto &row : zsdk::registry::Rows())
    {
        That(std::regex_match(row.id, std::regex("[a-z0-9_]+")) && std::regex_match(row.tag, std::regex("[A-Za-z0-9]+")), std::string("new-addon can read the row: ") + row.id);
        That(row.lane != zsdk::registry::kSpareLane, std::string("no row takes the spare lane: ") + row.id);
    }
    const zsdk::registry::Row *pulse = zsdk::registry::Find("pulse");
    That(pulse && std::string(pulse->tag) == "ZonePulse" && pulse->lane == 0.60f && std::string(pulse->keys) == "ALT+SHIFT+J|ALT+J|ALT+K", "the ZonePulse row is the row ZonePulse carries itself");
    That(zsdk::app::Row() && std::string(zsdk::app::Row()->tag) == "ZoneTemplate" && zsdk::app::Lane() == 0.54f, "an add-on without its own row falls back to the table row of its registry id");
    zsdk::App &app = probe::Mutable();
    app.identity.registry = "nobody";
    That(zsdk::app::Row() == nullptr && zsdk::app::Lane() == zsdk::registry::kSpareLane, "an add-on with no row at all draws its toasts on the spare lane, not on another add-on's lane");
    app.row = zsdk::registry::Row{ "quiet", "ZoneSdkProbe", 0.0f, "" };
    That(zsdk::app::Row() && zsdk::app::Lane() == 0.0f, "lane 0 in the add-on's own row means no toasts");
    app.row = zsdk::registry::Row{ nullptr, nullptr, 0.0f, nullptr };
    app.identity.registry = "template";
    const zsdk::Identity &id = zsdk::app::Get().identity;
    That(zsdk::WakeName(id) == L"ZoneSdkProbe.GameThread.1" && zsdk::MutexShape(id) == L"Local\\ZoneSdkProbe.Addon.%lu", "process names derive from the tag");
    zsdk::Identity other = id;
    other.name = "My Hud";
    other.tag = "JaneHud";
    That(zsdk::WakeName(other) == L"JaneHud.GameThread.1" && zsdk::MutexShape(other) == L"Local\\JaneHud.Addon.%lu", "a tag differing from the name names the process objects");
    zsdk::Policy wide;
    wide.watchKept = 32;
    That(!zsdk::Legacy(wide) && zsdk::LegacyText(wide) == "none", "watchKept is not a legacy knob");
    return check::Finish("policy");
}
