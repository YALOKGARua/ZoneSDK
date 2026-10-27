#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "fieldwatch.h"

#include <string>

int main()
{
    using check::That;
    using zfieldwatch::Settings;
    namespace fields = zfieldwatch::fields;
    Settings s;
    s.language = "en";
    s.enabled = false;
    s.logLevel = 2;
    const std::string text = zsdk::ini::Text(s);
    Settings back;
    zsdk::ini::Parse(text, back);
    That(back == s, "the settings survive a round trip");
    That(text.find("[general]\r\nlanguage=en\r\nenabled=0\r\nlog_level=2\r\n") != std::string::npos, "the settings are written in the general section");
    const zsdk::App &app = zsdk::app::Get();
    const zsdk::registry::Row *row = zsdk::app::Row();
    That(row && std::string(row->tag) == app.identity.tag && std::string(row->id) == app.identity.registry, "the add-on row names the add-on");
    That(row && row->lane == 0.0f && std::string(row->keys).empty(), "the sample has no toast lane and no key, it shows everything in its tab");
    That(app.policy == zsdk::Policy{} && !zsdk::Legacy(app.policy), "the sample runs the SDK default policy");
    That(app.specs.size() == zsdk::verbs::common::kWorldCount && app.specs.data() == zsdk::verbs::common::kWorld, "the verb table is the SDK world catalog");
    That(app.world.camera == zsdk::verbs::common::kCameraManager && app.world.frames == zsdk::verbs::common::kFrameCount, "the world verbs are the catalog indices");
    That(app.sets.size() == 2 && app.sets[0] == &fields::kCamera61bc1e03 && app.sets[1] == &fields::kCamera2bd3f115, "App.sets lists the guard set of each build, so the SDK checks them at prime and the guards suite compares them");
    That(app.census.staticCount == 2 && app.census.statics == zsdk::verbs::common::kStatics, "the census looks for the two statics of the world verbs");
    for (const char *id : { "title", "wait.world", "mod.enabled", "mod.paused", "head.fields", "field.line", "value.fov", "value.free", "value.locked", "proof.reflected", "proof.guard", "proof.unproven",
                            "verdict.pending", "verdict.waiting", "verdict.off", "verdict.crashed", "verdict.unmarked", "verdict.unproven", "marker.fresh", "marker.proven", "marker.crashed",
                            "head.build", "build.unchecked", "build.known", "build.other", "build.rejected", "set.on", "set.off", "set.pending", "need.known", "phase.lost", "diag.title", "lang.auto" })
        That(zsdk::lang::Has(id), std::string("the tab has a text for ") + id);
    That(zsdk::lang::Format(zsdk::lang::Text("proof.guard", zsdk::lang::Lang::En), { "0x22B5CDE", "0x2CC", "fieldwatch.camera.2bd3f115", "2bd3f115" }) ==
             "Byte guard: the GetFOVAngle code at 0x22B5CDE holds +0x2CC. The set fieldwatch.camera.2bd3f115 needs exactly build 2bd3f115",
         "the guard row names the code, the offset, the set and the build");
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    That(zsdk::ini::Open(home / "data"), "the data folder opens");
    That(zsdk::ini::Config<Settings>() == Settings{}, "a fresh folder has the default settings");
    That(scan::Body(home / "data" / "fieldwatch.ini").find("[general]\r\nlanguage=auto\r\nenabled=1\r\nlog_level=1\r\n") == 0, "the ini is written, the language follows the game");
    That(!zsdk::ledger::Blocked(zfieldwatch::kDefaultKey) && !zsdk::ledger::Blocked(zfieldwatch::kLockedKey), "no read is disabled in a fresh folder");
    return check::Finish("local");
}
