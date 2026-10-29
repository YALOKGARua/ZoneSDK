#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "template.h"

#include <string>

int main()
{
    using check::That;
    using ztemplate::Settings;
    namespace common = zsdk::verbs::common;
    Settings s;
    s.language = "en";
    s.toasts = false;
    s.locate = zsdk::keys::Parse("ALT+K");
    const std::string text = zsdk::ini::Text(s);
    Settings back;
    zsdk::ini::Parse(text, back);
    That(back == s, "the template settings survive a round trip");
    That(text.find("[keys]\r\nlocate=ALT+K\r\n") != std::string::npos, "the locate key is written by name");
    That(Settings{}.locate.mods == zsdk::keys::kAlt && Settings{}.locate.key == 'L' && !zsdk::keys::Numpad(Settings{}.locate.key), "the default key is Alt+L, not a numpad key");
    const zsdk::App &app = zsdk::app::Get();
    const zsdk::registry::Row *row = zsdk::app::Row();
    That(row == &app.row, "the add-on carries its own registry row");
    That(row && zsdk::keys::Parse(row->keys) == Settings{}.locate, "the add-on registry row carries the default key");
    That(row && std::string(row->tag) == app.identity.tag && std::string(row->id) == app.identity.registry, "the add-on row names the add-on");
    That(row && zsdk::app::Lane() == row->lane, "toasts are drawn on the add-on's own lane, not on the spare lane");
    const zsdk::registry::Row *listed = row ? zsdk::registry::Find(row->id) : nullptr;
    That(!listed || (listed->lane == row->lane && std::string(listed->keys) == row->keys), "the add-on row agrees with the ZoneSDK table row of its id on lane and key");
    That(app.policy == zsdk::Policy{} && !zsdk::Legacy(app.policy), "the template runs the SDK default policy");
    bool roles = true;
    const int world[] = { app.world.controller, app.world.pawn, app.world.camera, app.world.seconds, app.world.paused, app.world.frames };
    for (int role = 0; role < common::kWorldCount; ++role) roles = roles && common::Plays(app.specs, world[role], static_cast<common::World>(role));
    That(roles && app.specs.size() == ztemplate::kSpecs.size(), "the world verbs are the shared catalog at the head of the table");
    That(ztemplate::kSpecs[ztemplate::kLocation].mark == zsdk::verbs::Mark::Risky && zsdk::verbs::Index(app.specs, "Actor", "K2_GetActorLocation") == ztemplate::kLocation, "the guarded call is marked risky and follows the catalog");
    bool own = false;
    for (const auto id : zsdk::lang::Ids()) own = own || id.starts_with("diag.") || id.starts_with("phase.") || id == "key.press" || id == "key.bad" || id == "key.game" || id == "yes" || id == "no";
    That(!own && zsdk::lang::Has("phase.lost") && zsdk::lang::Has("diag.pump") && zsdk::lang::Has("key.press") && zsdk::lang::Has("diag.retry"), "the template has no diagnostics, phase or key rows of its own, the SDK defaults fill them");
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    That(zsdk::ini::Open(home / "data"), "the data folder opens");
    That(zsdk::ini::Config<Settings>() == Settings{}, "a fresh folder has the default settings");
    That(scan::Body(home / "data" / "template.ini").find("[general]\r\nlanguage=auto\r\nenabled=1\r\ntoasts=1\r\nlog_level=1\r\n") == 0, "the ini is written, the language follows the game");
    That(!zsdk::ledger::Blocked(ztemplate::kLocateKey), "the locate call is not disabled");
    That(zsdk::lang::Has("say.located") && zsdk::lang::Format(zsdk::lang::Text("say.located", zsdk::lang::Lang::En), { "1 2 3" }) == "You are here: 1 2 3", "the toast text");
    return check::Finish("local");
}
