#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/engine_core.h"
#include "../src/detail/runtime.h"
#include "support/fake.h"

#include <string>

namespace
{
    using check::That;
    namespace ue = zsdk::ue;
    namespace engine = zsdk::detail::engine;
    namespace call = zsdk::detail::call;

    template<class F> std::string Refusal(F &&work)
    {
        try
        {
            work();
            return "passed";
        }
        catch (const zsdk::Refused &error)
        {
            return std::string("refused: ") + error.what();
        }
        catch (const std::exception &error)
        {
            return std::string("error: ") + error.what();
        }
    }

    ue::Param P(const char *name, const char *type, int32_t size)
    {
        ue::Param p;
        p.name = name;
        p.type = type;
        p.size = size;
        return p;
    }

    bool Has(const std::string &text, const std::string &part)
    {
        return text.find(part) != std::string::npos;
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path logFile = home / "objects.log";
    zsdk::log::Open(logFile, home / "objects.1.log");

    fake::Game game;
    That(ue::Total() == 3 && ue::Label(game.klass) == "Class" && ue::Label(ue::ClassOf(game.object)) == "Class", "the fake object array is posed");

    uint8_t *actorType = game.Type("Actor", game.object);
    uint8_t *alive = game.Make("BP_Alive", actorType, nullptr);
    uint8_t *garbage = game.Make("BP_Garbage", actorType, nullptr);
    uint8_t *unreachable = game.Make("BP_Unreachable", actorType, nullptr);
    uint8_t *destroyed = game.Make("BP_Destroyed", actorType, nullptr);
    uint8_t *counted = game.Make("BP_Counted", actorType, nullptr);
    uint8_t *renamed = game.Make("BP_Renamed", actorType, nullptr);
    game.ItemFlags(garbage, ue::kItemGarbage);
    game.ItemFlags(unreachable, ue::kItemUnreachable);
    game.ObjectFlags(destroyed, 0x00008000u);
    game.ItemFlags(counted, 0x20000000u);

    That(ue::Here(alive) && !ue::Doomed(alive), "a live object is not doomed");
    That(ue::Here(garbage) && ue::Doomed(garbage), "an item marked Garbage is doomed although it is still in the array");
    That(ue::Here(unreachable) && ue::Doomed(unreachable), "an item marked Unreachable is doomed");
    That(ue::Here(destroyed) && ue::Doomed(destroyed), "an object that began its destroy is doomed");
    That(ue::Here(counted) && !ue::Doomed(counted), "item bit 29 alone (RefCounted in UE 5.3+, PendingKill in old UE) is not doomed");
    That(ue::Doomed(nullptr) && ue::Doomed(game.Loose()), "null and memory outside the object array are doomed");
    That(ue::Here(alive, actorType) && !ue::Here(alive, game.object), "Here keeps its class check");

    const ue::Mark mark = ue::Seal(renamed);
    That(ue::Ours(mark), "a sealed live object is ours");
    game.Serial(renamed, 4242);
    That(!ue::Ours(mark) && !ue::Doomed(renamed), "a different serial is not ours, the object itself is alive");
    game.Serial(renamed, mark.serial);
    That(ue::Ours(mark), "the serial back, ours again");
    game.ItemFlags(renamed, ue::kItemUnreachable);
    That(!ue::Ours(mark), "Ours refuses a doomed object whose identity still matches");
    game.ItemFlags(renamed, 0);
    That(!ue::Ours(ue::Seal(garbage)) && !ue::Ours(ue::Seal(destroyed)) && ue::Ours(ue::Seal(counted)), "Ours is the identity check and not doomed");

    const std::string seen = ue::Look(garbage);
    That(Has(seen, "BP_Garbage") && Has(seen, "object flags 0x00000000") && Has(seen, "item flags 0x00200000") && Has(seen, "cluster 0") && Has(seen, "doomed"), "Look names the flags: " + seen);
    That(Has(ue::Look(alive), "alive") && Has(ue::Look(destroyed), "object flags 0x00008000") && Has(ue::Look(game.Loose()), "not in the object array") && ue::Look(nullptr) == "null",
         "Look covers alive, destroyed, outside and null");

    const std::string key = "Actor.K2_GetActorLocation";
    uint8_t *location = game.Make("K2_GetActorLocation", game.function, actorType);
    That(Refusal([&] { call::Aim(key, nullptr, true, actorType, location); }) == "refused: host not ready: " + key, "a call without a host is refused as not ready, so the add-on can retry");
    That(Refusal([&] { call::Aim(key, alive, true, actorType, location); }) == "passed", "a live named target of the right class binds");
    That(Refusal([&] { call::Aim(key, garbage, true, actorType, location); }) == "refused: " + key + " refused for a doomed object", "a doomed named target is refused");
    That(Refusal([&] { call::Aim(key, destroyed, true, actorType, location); }) == "refused: " + key + " refused for a doomed object", "a begin-destroyed named target is refused");
    That(Refusal([&] { call::Aim(key, counted, true, actorType, location); }) == "passed", "bit 29 alone does not refuse a target");
    That(Refusal([&] { call::Aim(key, destroyed, false, actorType, location); }) == "passed", "with verifiedOnly off the legacy call skips the doomed check");

    uint8_t *pawnType = game.Type("Pawn", actorType);
    uint8_t *movement = game.Type("NavMovementInterface", game.object);
    uint8_t *walker = game.Type("Walker", pawnType);
    uint8_t *stone = game.Type("Stone", game.object);
    game.Implements(walker, movement, 0x28);
    uint8_t *walk = game.Make("XRequestMove", game.function, movement);
    uint8_t *npc = game.Make("Walker_C", walker, nullptr);
    uint8_t *rock = game.Make("Stone_C", stone, nullptr);
    const std::string moveKey = "NavMovementInterface.XRequestMove";
    That(engine::Facet(npc, walk) == npc + 0x28, "Facet finds the interface table of the class");
    That(call::Fits(npc, movement, walk) && !call::Fits(rock, movement, walk) && call::Fits(npc, actorType, location), "an interface function fits a class that implements it");
    That(Refusal([&] { call::Aim(moveKey, npc, true, movement, walk); }) == "passed", "Bind accepts an interface function through Facet");
    That(Refusal([&] { call::Aim(moveKey, rock, true, movement, walk); }) == "error: " + moveKey + " refused for Stone", "a class without the interface is still refused");
    That(Refusal([&] { call::Aim(key, rock, true, actorType, location); }) == "error: " + key + " refused for Stone", "a wrong class is refused as before");

    const void *pointer = garbage;
    const void *fine = alive;
    const void *none = nullptr;
    alignas(16) uint8_t face[16]{};
    std::memcpy(face, &pointer, sizeof(pointer));
    That(Refusal([&] { call::Pass(key, P("Target", "ObjectProperty", 8), &pointer, 8); }) == "refused: parameter Target of " + key + " refused for a doomed object", "an object parameter refuses a doomed pointer");
    That(Refusal([&] { call::Pass(key, P("Kind", "ClassProperty", 8), &pointer, 8); }) == "refused: parameter Kind of " + key + " refused for a doomed object", "a class parameter refuses a doomed pointer");
    That(Refusal([&] { call::Pass(key, P("Face", "InterfaceProperty", 16), face, 16); }) == "refused: parameter Face of " + key + " refused for a doomed object", "an interface parameter refuses a doomed object");
    That(Refusal([&] { call::Pass(key, P("Target", "ObjectProperty", 8), &fine, 8); }) == "passed" && Refusal([&] { call::Pass(key, P("Target", "ObjectProperty", 8), &none, 8); }) == "passed",
         "a live pointer and null pass");
    That(Refusal([&] { call::Pass(key, P("Count", "Int64Property", 8), &pointer, 8); }) == "passed" && Refusal([&] { call::Pass(key, P("Soft", "SoftObjectProperty", 8), &pointer, 8); }) == "passed",
         "other parameter types are not read as pointers");

    uint8_t *library = game.Type("BlueprintFunctionLibrary", game.object);
    uint8_t *statics = game.Type("GameplayStatics", library);
    uint8_t *staticsCdo = game.Make("Default__GameplayStatics", statics, nullptr);
    That(engine::Host(statics) == staticsCdo, "a function library keeps its class default object as host");
    uint8_t *console = game.Type("CustomConsoleManagerRK", game.object);
    game.Make("Default__CustomConsoleManagerRK", console, nullptr);
    That(engine::Host(console) == nullptr, "a class default object of a host class is refused");
    zsdk::Flush();
    That(Has(scan::Body(logFile), "engine: host for CustomConsoleManagerRK not ready"), "the refusal is logged as host not ready");
    uint8_t *live = game.Make("CustomConsoleManagerRK", console, nullptr, 1);
    That(engine::Host(console) == nullptr, "the host is not searched again inside the retry window");
    engine::ClearHosts();
    That(engine::Host(console) == live, "ClearHosts empties the cache, the next lookup finds the live host");
    That(engine::Host(console) == live, "the cached host is reused");
    game.ObjectFlags(live, 0x00008000u);
    That(engine::Host(console) == nullptr, "a cached host that began its destroy is not reused");
    engine::ClearHosts();
    That(engine::Host(console) == nullptr, "a doomed object is never taken as a new host");
    game.ObjectFlags(live, 0);
    engine::ClearHosts();
    That(engine::Host(console) == live, "the host is found again once it is alive");

    zsdk::world::Grip &grip = zsdk::world::Hold();
    grip.epoch = 0;
    grip.since = 0;
    That(zsdk::census::Host(0) == nullptr && !zsdk::census::Consoles(), "no census host before the first sweep");
    uint64_t sweptAt = 0;
    uint32_t sweptEpoch = 0xFFFFFFFFu;
    zsdk::detail::census::Tally(sweptAt, sweptEpoch);
    const auto snap = zsdk::census::Current();
    That(snap && snap->hosts[0] == live && snap->statics[0] == staticsCdo, "the census sweep over the fake array finds the console host and the static");
    That(zsdk::census::Fresh(*snap, 0, 0) && !zsdk::census::Fresh(*snap, 1, 0) && !zsdk::census::Fresh(*snap, 0, snap->at + 1), "a snapshot is fresh only for its epoch and at or after the world start");
    That(zsdk::census::Host(0) == live && zsdk::census::Consoles(), "a fresh census host is used");
    That(zsdk::census::Host(1) == nullptr && zsdk::census::Host(99) == nullptr, "slots outside the census spec give nothing");
    grip.since = snap->at + 60000;
    That(zsdk::census::Host(0) == nullptr && !zsdk::census::Consoles(), "a census host from before the world start is not used (wait.consoles)");
    grip.since = 0;
    grip.epoch = 7;
    That(zsdk::census::Host(0) == nullptr, "a census host from another world epoch is not used");
    zsdk::detail::census::Turn(7);
    zsdk::detail::census::Tally(sweptAt, sweptEpoch);
    That(zsdk::census::Current()->epoch == 7 && zsdk::census::Host(0) == live && zsdk::census::Consoles(), "the sweep of the new epoch brings the host back");
    game.ItemFlags(live, ue::kItemGarbage);
    That(zsdk::census::Host(0) == nullptr, "a doomed census host is not used");
    game.ItemFlags(live, 0);

    uint8_t *worldType = game.Type("World", game.object);
    uint8_t *levelType = game.Type("Level", game.object);
    game.Property(levelType, "OwningWorld", 0x100);
    uint8_t *persistent = game.Make("WorldMap_WP", worldType, nullptr);
    uint8_t *cell = game.Make("WorldMap_WP_MainGrid_L0_X1_Y2", worldType, nullptr);
    uint8_t *level = game.Make("PersistentLevel", levelType, cell);
    game.Link(level, 0x100, persistent);
    uint8_t *flare = game.Make("BP_Flare", actorType, level);
    uint8_t *mesh = game.Make("Mesh", game.object, flare);
    That(ue::PropertyOffset(levelType, "OwningWorld") == 0x100, "the fake level class reflects OwningWorld");
    That(zsdk::world::WorldOf(flare) == cell, "WorldOf returns the outer world, here the cell world");
    That(zsdk::world::OwningWorld(flare) == persistent && zsdk::world::OwningWorld(mesh) == persistent && zsdk::world::OwningWorld(level) == persistent,
         "OwningWorld returns the persistent world through the level");
    That(zsdk::world::OwningWorld(cell) == nullptr && zsdk::world::OwningWorld(nullptr) == nullptr, "a world has no level outer");
    game.ObjectFlags(flare, 0x00000400u);
    That(zsdk::world::OwningWorld(flare) == nullptr && zsdk::world::OwningWorld(mesh) == nullptr, "an object that is still loading (0x3400) is skipped");
    game.ObjectFlags(flare, 0);
    game.ObjectFlags(level, 0x00001000u);
    That(zsdk::world::OwningWorld(flare) == nullptr, "a level that is still loading is skipped");
    game.ObjectFlags(level, 0);
    game.ItemFlags(persistent, ue::kItemUnreachable);
    That(zsdk::world::OwningWorld(flare) == nullptr, "a doomed owning world is not returned");
    game.ItemFlags(persistent, 0);
    That(zsdk::world::OwningWorld(flare) == persistent, "and it comes back when alive");

    const uint32_t probe = game.Name("XProbe");
    That(ue::NameText(probe, 0) == "XProbe" && ue::NameText(probe, 1) == "XProbe_0" && ue::NameText(probe, 13) == "XProbe_12", "NameText decodes the number suffix");
    That(ue::NameText(0, 5).empty() && ue::Label(live) == "CustomConsoleManagerRK_0", "an empty name has no suffix, Label agrees with NameText");

    zsdk::Flush();
    return check::Finish("objects");
}
