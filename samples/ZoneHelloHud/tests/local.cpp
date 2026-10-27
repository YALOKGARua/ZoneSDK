#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "hellohud.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <string>
#include <thread>

namespace
{
    using check::That;
    using zhellohud::Corner;
    using zhellohud::Settings;
    using zhellohud::Status;
    using zhellohud::Unit;
    namespace view = zhellohud::view;
    namespace lang = zsdk::lang;
    namespace common = zsdk::verbs::common;

    bool Near(double a, double b)
    {
        return std::fabs(a - b) < 1e-9;
    }

    Status Live(uint64_t when)
    {
        Status st;
        st.phase = zsdk::Phase::Ready;
        st.live = true;
        st.reading.at = { .x = 123456.0, .y = -7890.0, .z = 4.0 };
        st.reading.velocity = { .x = 300.0, .y = 400.0, .z = 999.0 };
        st.reading.rot = { .pitch = 10.0, .yaw = 0.0, .roll = 0.0 };
        st.reading.moving = true;
        st.reading.turned = true;
        st.reading.when = when;
        return st;
    }

    void Store()
    {
        Settings s;
        s.language = "en";
        s.toasts = false;
        s.shown = false;
        s.corner = Corner::BottomRight;
        s.unit = Unit::Ms;
        s.toggle = zsdk::keys::Parse("ALT+K");
        const std::string text = zsdk::ini::Text(s);
        Settings back;
        zsdk::ini::Parse(text, back);
        That(back == s, "the settings survive a round trip");
        That(text.find("[hud]\r\nshown=0\r\ncorner=bottom_right\r\nspeed=ms\r\n") != std::string::npos, "the corner and the speed unit are written as words");
        That(text.find("[keys]\r\ntoggle=ALT+K\r\n") != std::string::npos, "the toggle key is written by name");
        Settings odd;
        zsdk::ini::Parse("[hud]\r\ncorner=middle\r\nspeed=KMH\r\n", odd);
        odd = zsdk::ini::Sanitize(odd);
        That(odd.corner == Corner::TopLeft && odd.unit == Unit::Kmh, "an unknown corner falls back to top_left, and words are read without case");
        const zsdk::keys::Chord key = Settings{}.toggle;
        That(key.mods == zsdk::keys::kAlt && key.key == '7' && !zsdk::keys::Numpad(key.key) && zsdk::keys::Bindable(key), "the default key is Alt+7, not a numpad key");
        That(zsdk::keys::Game('7'), "a bare 7 belongs to the game, so the default needs Alt");
    }

    void App()
    {
        const zsdk::App &a = zsdk::app::Get();
        const zsdk::registry::Row *row = zsdk::app::Row();
        That(row && zsdk::keys::Parse(row->keys) == Settings{}.toggle, "the registry row carries the default key");
        That(row && std::string(row->tag) == a.identity.tag && std::string(row->id) == a.identity.registry, "the registry row names the add-on");
        That(row && Near(row->lane, 0.66f) && Near(zsdk::app::Lane(), 0.66f), "the toasts use the lane of the row");
        That(a.policy == zsdk::Policy{} && !zsdk::Legacy(a.policy), "the sample runs the SDK default policy");
        That(a.specs.data() == zhellohud::kSpecs.data() && a.specs.size() == zhellohud::kVerbs, "the app table is the verb table");
        const int roles[] = { a.world.controller, a.world.pawn, a.world.camera, a.world.seconds, a.world.paused, a.world.frames };
        bool plays = true;
        for (int role = 0; role < common::kWorldCount; ++role) plays = plays && roles[role] == role && common::Plays(a.specs, roles[role], static_cast<common::World>(role));
        That(plays, "WorldVerbs{} points at the shared world catalog that starts the table");
        That(a.census.statics == common::kStatics && a.census.staticCount == 2, "the census looks for the shared statics");
        const zsdk::verbs::Spec &where = zhellohud::kSpecs[zhellohud::kLocation];
        const zsdk::verbs::Spec &pace = zhellohud::kSpecs[zhellohud::kVelocity];
        const zsdk::verbs::Spec &turn = zhellohud::kSpecs[zhellohud::kRotation];
        That(where.required && !pace.required && !turn.required, "only the location is required, speed and heading lines drop out on their own");
        That(where.mark == zsdk::verbs::Mark::Risky && pace.mark == zsdk::verbs::Mark::Risky && turn.mark == zsdk::verbs::Mark::Risky, "every reflected call is marked risky");
        bool actor = true;
        for (const zsdk::verbs::Spec *one : { &where, &pace, &turn })
            actor = actor && std::string(one->owner) == "Actor" && std::string(one->params[0].name) == "ReturnValue" && one->params[0].size == 24 && !one->params[1].name;
        That(actor, "each call is an Actor function that returns one 24-byte struct and takes nothing");
    }

    void Compass()
    {
        That(Near(view::Bearing(-90.0), 0.0) && Near(view::Bearing(0.0), 90.0) && Near(view::Bearing(90.0), 180.0) && Near(view::Bearing(180.0), 270.0), "the bearing turns yaw into a compass with north at -Y");
        That(Near(view::Bearing(-180.0), 270.0) && Near(view::Bearing(630.0), 0.0) && Near(view::Bearing(-450.0), 0.0), "the bearing wraps any yaw into 0 to 360");
        That(std::string(view::Point(0.0)) == "dir.n" && std::string(view::Point(22.4)) == "dir.n" && std::string(view::Point(22.6)) == "dir.ne", "north spans 45 degrees around 0");
        That(std::string(view::Point(90.0)) == "dir.e" && std::string(view::Point(180.0)) == "dir.s" && std::string(view::Point(270.0)) == "dir.w" && std::string(view::Point(315.0)) == "dir.nw", "the main points");
        That(std::string(view::Point(337.6)) == "dir.n" && std::string(view::Point(359.9)) == "dir.n" && std::string(view::Point(-10.0)) == "dir.n", "the point wraps around north");
        bool named = true;
        for (const char *id : { "dir.n", "dir.ne", "dir.e", "dir.se", "dir.s", "dir.sw", "dir.w", "dir.nw" }) named = named && lang::Has(id);
        That(named, "every compass point has a row");
        const zsdk::math::Vec3 v{ .x = 300.0, .y = 400.0, .z = 999.0 };
        That(Near(view::Pace(v, Unit::Ms), 5.0) && Near(view::Pace(v, Unit::Kmh), 18.0), "the speed is flat, 500 cm/s are 5 m/s and 18 km/h");
        That(view::Fixed(1234.56, 1) == "1234.6" && view::Fixed(-0.04, 1) == "0.0" && view::Fixed(-7.25, 1) == "-7.3" && view::Fixed(2.0, 0) == "2", "numbers are rounded and never show -0.0");
    }

    void Lines()
    {
        const Status st = Live(1000);
        That(view::Fresh(st, 1500) && view::Fresh(st, 2000) && !view::Fresh(st, 2001), "a reading is fresh for one second");
        That(view::Fresh(st, 900), "a reading stamped after now counts as age 0");
        Status none = st;
        none.reading.when = 0;
        Status off = st;
        off.live = false;
        That(!view::Fresh(none, 1000) && !view::Fresh(off, 1000), "no reading or a hidden panel is never fresh");
        const std::vector<std::string> en = view::Lines(st, Unit::Kmh, 1100, lang::Lang::En);
        That(en.size() == 3 && en[0] == "X 1234.6  Y -78.9  Z 0.0 m", "the first line has the position in metres: " + (en.empty() ? std::string() : en[0]));
        That(en.size() == 3 && en[1] == "Heading 90° E" && en[2] == "Speed 18.0 km/h", "the heading and the speed lines in English");
        const std::vector<std::string> uk = view::Lines(st, Unit::Ms, 1100, lang::Lang::Uk);
        That(uk.size() == 3 && uk[1] == "Напрямок 90° Сх" && uk[2] == "Швидкість 5.0 м/с",
             "the heading and the speed lines in Ukrainian, speed in m/s");
        Status bare = st;
        bare.reading.moving = false;
        bare.reading.turned = false;
        That(view::Lines(bare, Unit::Kmh, 1100, lang::Lang::En).size() == 1, "without speed and heading only the position line is left");
        Status turned = st;
        turned.reading.rot.yaw = -90.4;
        const std::vector<std::string> north = view::Lines(turned, Unit::Kmh, 1100, lang::Lang::En);
        That(north.size() == 3 && north[1] == "Heading 0° N", "a bearing of 359.6 is shown as 0, north");
        Status waiting = st;
        waiting.phase = zsdk::Phase::Waiting;
        waiting.reading.when = 0;
        const std::vector<std::string> wait = view::Lines(waiting, Unit::Kmh, 1100, lang::Lang::En);
        That(wait.size() == 1 && wait[0] == lang::Format(lang::Text("hud.wait", lang::Lang::En), { view::State(waiting, lang::Lang::En) }) &&
                 wait[0].find(lang::Text("wait.world", lang::Lang::En)) != std::string::npos,
             "while the world loads the panel names the phase and the wait");
        Status refused = st;
        refused.reading.when = 0;
        refused.refusal = "why.world";
        That(view::Lines(refused, Unit::Kmh, 1100, lang::Lang::En) == std::vector<std::string>{ "ZoneHelloHud: the game world is not ready" }, "a refusal is shown with its text");
        refused.refusal.clear();
        That(view::Lines(refused, Unit::Kmh, 1100, lang::Lang::En) == std::vector<std::string>{ "ZoneHelloHud: reading the game" }, "a ready world without a reading yet");
        That(view::Lines(off, Unit::Kmh, 1100, lang::Lang::En) == std::vector<std::string>{ "The panel is hidden" }, "a hidden panel says so in the tab");
    }

    void Corners()
    {
        const view::Spot tl = view::Place(Corner::TopLeft, 1920.0f, 1080.0f, 200.0f, 100.0f, 24.0f);
        const view::Spot tr = view::Place(Corner::TopRight, 1920.0f, 1080.0f, 200.0f, 100.0f, 24.0f);
        const view::Spot bl = view::Place(Corner::BottomLeft, 1920.0f, 1080.0f, 200.0f, 100.0f, 24.0f);
        const view::Spot br = view::Place(Corner::BottomRight, 1920.0f, 1080.0f, 200.0f, 100.0f, 24.0f);
        That(tl.x == 24.0f && tl.y == 24.0f && tr.x == 1696.0f && tr.y == 24.0f, "the top corners keep the edge");
        That(bl.x == 24.0f && bl.y == 956.0f && br.x == 1696.0f && br.y == 956.0f, "the bottom corners keep the edge");
        const view::Spot big = view::Place(Corner::BottomRight, 320.0f, 240.0f, 400.0f, 300.0f, 24.0f);
        That(big.x == 0.0f && big.y == 0.0f, "a panel larger than the screen starts at the top left");
    }

    void Rows()
    {
        bool own = false;
        for (const auto id : lang::Ids()) own = own || id.starts_with("diag.") || id.starts_with("phase.") || id.starts_with("key.press") || id == "yes";
        That(!own && lang::Has("diag.title") && lang::Has("key.bad") && lang::Has("phase.ready"), "the sample has no diagnostics, phase or key rows of its own, the SDK defaults fill them");
        That(std::string(lang::Text("hud.where", lang::Lang::Uk)) == "X {0}  Y {1}  Z {2} м" && std::string(lang::Text("say.hidden", lang::Lang::En)) == "The panel is hidden", "the UA and EN rows");
    }

    void Core(const std::filesystem::path &home)
    {
        namespace core = zhellohud::core;
        That(zsdk::ini::Open(home / "data"), "the data folder opens");
        That(zsdk::ini::Config<Settings>() == Settings{}, "a fresh folder has the default settings");
        const std::string fresh = scan::Body(home / "data" / "hellohud.ini");
        That(fresh.find("[general]\r\nlanguage=auto\r\nenabled=1\r\ntoasts=1\r\nlog_level=1\r\n") == 0, "the ini is written, the language follows the game");
        That(fresh.find("[hud]\r\nshown=1\r\ncorner=top_left\r\nspeed=kmh\r\n") != std::string::npos && fresh.find("[keys]\r\ntoggle=ALT+7\r\n") != std::string::npos, "the panel and key sections are written");
        That(std::string(core::OrderName(static_cast<int>(zhellohud::Order::Toggle))) == "toggle" && std::string(core::OrderName(7)) == "unknown", "the order names");
        core::Refresh(true);
        zsdk::toast::Reset();
        core::Order(static_cast<int>(zhellohud::Order::Toggle));
        That(!zsdk::ini::Config<Settings>().shown, "the toggle order hides the panel in the settings");
        That(zsdk::ini::Save() && scan::Body(home / "data" / "hellohud.ini").find("[hud]\r\nshown=0\r\n") != std::string::npos, "the toggled setting is saved");
        core::Publish(zsdk::Now());
        That(!core::Snapshot().live && !core::Hot(), "a hidden panel publishes live off and keeps the game thread cold");
        core::Order(static_cast<int>(zhellohud::Order::Toggle));
        That(zsdk::ini::Config<Settings>().shown, "a second toggle shows it again");
        const std::vector<zsdk::toast::Item> live = zsdk::toast::Live(zsdk::Now());
        That(live.size() == 1 && live[0].kind == static_cast<int>(zhellohud::Say::Panel) && live[0].args == std::vector<std::string>{ "on" }, "the panel toast replaces the one before it");
        const uint64_t now = zsdk::Now();
        core::Step(now);
        That(!core::Hot(), "no world, so the game thread is not kept hot");
        core::Publish(now);
        const Status st = core::Snapshot();
        That(st.live && st.reads == 0 && st.refusal == "why.world" && !view::Fresh(st, now), "a step without a world refuses the reading and publishes why");
        That(core::Section().find("hellohud: panel on, 0 readings") == 0, "the report section");
        std::atomic<bool> done{ false };
        std::atomic<int> torn{ 0 };
        std::thread reader([&] {
            while (!done.load())
            {
                const Status seen = core::Snapshot();
                if (seen.refusal != "why.world" || seen.wait.empty() || seen.wait.find("wait.") != 0) torn.fetch_add(1);
            }
        });
        for (int i = 0; i < 20000; ++i) core::Publish(now + static_cast<uint64_t>(i));
        done = true;
        reader.join();
        That(torn.load() == 0, "a snapshot taken during 20000 publishes from another thread is always whole");
        zsdk::ini::Close();
    }
}

int main()
{
    Store();
    App();
    Compass();
    Lines();
    Corners();
    Rows();
    const wchar_t *given = _wgetenv(L"ZSDK_TEST_HOME");
    Core(given && *given ? std::filesystem::path(given) : std::filesystem::temp_directory_path());
    return check::Finish("local");
}
