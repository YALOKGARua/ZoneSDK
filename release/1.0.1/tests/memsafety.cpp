#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/engine_core.h"
#include "../src/detail/runtime.h"
#include "support/fake.h"

#include <windows.h>

#include <cstring>
#include <string>

namespace
{
    using check::That;
    namespace ue = zsdk::ue;
    namespace engine = zsdk::detail::engine;
    namespace call = zsdk::detail::call;

    template<class T> void Poke(uint8_t *at, int32_t offset, const T &value)
    {
        std::memcpy(at + offset, &value, sizeof(T));
    }

    int32_t IndexOf(const void *thing)
    {
        int32_t index = -1;
        std::memcpy(&index, static_cast<const uint8_t *>(thing) + ue::kIndexAt, sizeof(index));
        return index;
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    zsdk::log::Open(home / "memsafety.log", home / "memsafety.1.log");

    That(ue::kItemFlagsAt == 0x08 && ue::kItemClusterAt == 0x0C && fake::kItemSerialAt == 0x10 && fake::kStride == 24,
         "the FUObjectItem layout of 61bc1e03 and 2bd3f115: flags +0x08, cluster +0x0C, serial +0x10 in 24-byte items, as the exe's own serial compares read it");
    That(ue::kItemGarbage == (1u << 21) && ue::kItemUnreachable == (1u << 28) && (ue::kItemGarbage | ue::kItemUnreachable) == 0x10200000u,
         "Garbage is item bit 21 and Unreachable item bit 28, the mask the exe's own weak pointer check tests");
    That(ue::kObjectDoomed == (0x00008000u | 0x00010000u | 0x40000000u) && (ue::kObjectDoomed & 0x30u) == 0 && (ue::kGoneStrict & ue::kObjectDoomed) == ue::kObjectDoomed &&
             (ue::kGoneStrict & 0x30u) == 0x30u,
         "the doomed object mask is BeginDestroyed, FinishDestroyed and MirroredGarbage; only Gone carries the class-default bits");

    fake::Game game;
    uint8_t *actor = game.Type("Actor", game.object);
    uint8_t *finished = game.Make("BP_Finished", actor, nullptr);
    uint8_t *mirrored = game.Make("BP_Mirrored", actor, nullptr);
    uint8_t *defaults = game.Make("Default__BP_Thing", actor, nullptr);
    uint8_t *archetype = game.Make("BP_Archetype", actor, nullptr);
    uint8_t *rooted = game.Make("BP_Rooted", actor, nullptr);
    uint8_t *kept = game.Make("BP_Kept", actor, nullptr);
    uint8_t *both = game.Make("BP_Both", actor, nullptr);
    uint8_t *begun = game.Make("BP_Begun", actor, nullptr);
    game.ObjectFlags(finished, 0x00010000u);
    game.ObjectFlags(mirrored, 0x40000000u);
    game.ObjectFlags(defaults, 0x00000010u);
    game.ObjectFlags(archetype, 0x00000020u);
    game.ItemFlags(rooted, 0x40000000u);
    game.ItemFlags(kept, 0x6E500000u);
    game.ItemFlags(both, 0x10200000u);
    game.ObjectFlags(begun, 0x00008000u);

    That(ue::Doomed(finished) && ue::Doomed(mirrored), "FinishDestroyed alone and MirroredGarbage alone are doomed");
    That(!ue::Doomed(defaults) && !ue::Doomed(archetype) && ue::Gone(defaults) && ue::Gone(archetype), "a class default or archetype object is not doomed, so static calls keep their host, while Gone skips it");
    That(!ue::Doomed(rooted), "the RootSet item bit alone is not doomed");
    That(!ue::Doomed(kept), "the keep mask the exe tests (0x6E500000: RootSet, bit 29, native, async and loader bits) is not doomed");
    That(ue::Doomed(both) && ue::Doomed(begun), "Garbage with Unreachable, and BeginDestroyed, are doomed");
    That(ue::At(IndexOf(both)) == nullptr && ue::At(IndexOf(kept)) == kept && ue::At(IndexOf(rooted)) == rooted, "the object array walk drops Garbage or Unreachable items and keeps items with only keep bits");
    That(ue::At(IndexOf(begun)) == begun && ue::At(IndexOf(finished)) == finished, "the walk still returns begin-destroyed objects, so every caller of At needs its own Doomed check");

    uint8_t *moved = game.Make("BP_Moved", actor, nullptr);
    uint8_t *other = game.Make("BP_Other", actor, nullptr);
    Poke(moved, ue::kIndexAt, IndexOf(other));
    That(!ue::Here(moved) && ue::Doomed(moved) && ue::Here(other) && !ue::Doomed(other), "an object whose index names another object's slot is not here and is doomed");
    Poke(moved, ue::kIndexAt, int32_t{ 4000 });
    That(!ue::Here(moved) && ue::Doomed(moved), "an index past the end of the array is doomed");
    Poke(moved, ue::kIndexAt, int32_t{ -1 });
    That(!ue::Here(moved) && ue::Doomed(moved) && ue::Look(moved).find("not in the object array") != std::string::npos, "a negative index is doomed and Look says why");

    uint8_t *sealed = game.Make("BP_Sealed", actor, nullptr);
    const ue::Mark mark = ue::Seal(sealed);
    That(ue::Ours(mark) && mark.serial == IndexOf(sealed) + 1 && mark.index == IndexOf(sealed), "a sealed object is ours, the seal keeps the item serial from +0x10");
    uint64_t name = 0;
    std::memcpy(&name, sealed + ue::kNameAt, sizeof(name));
    const uint32_t renamed[2] = { game.Name("BP_Sealed"), 7 };
    std::memcpy(sealed + ue::kNameAt, renamed, sizeof(renamed));
    That(!ue::Ours(mark) && ue::Here(sealed), "another name number in the same slot is not ours");
    std::memcpy(sealed + ue::kNameAt, &name, sizeof(name));
    That(ue::Ours(mark), "the old name back, ours again");
    uint8_t *pawn = game.Type("Pawn", actor);
    Poke(sealed, ue::kClassAt, static_cast<const void *>(pawn));
    That(!ue::Ours(mark) && ue::Here(sealed), "another class at the same address is not ours");
    Poke(sealed, ue::kClassAt, static_cast<const void *>(actor));
    That(ue::Ours(mark), "the class back, ours again");
    game.Serial(sealed, 0);
    That(!ue::Ours(mark), "a slot whose serial changed to 0 is not ours");
    const ue::Mark unnumbered = ue::Seal(sealed);
    That(unnumbered.serial == 0 && ue::Ours(unnumbered), "an object sealed before the engine gave it a serial is ours by index, name and class");
    game.Serial(sealed, 99);
    That(ue::Ours(unnumbered), "a serial the engine gives it later keeps it ours");
    That(!ue::Ours(ue::Seal(nullptr)) && !ue::Ours(ue::Mark{}), "an empty seal is never ours");

    uint8_t *move = game.Type("NavMovementInterface", game.object);
    uint8_t *team = game.Type("GenericTeamAgentInterface", game.object);
    uint8_t *walker = game.Type("Walker", pawn);
    uint8_t *runner = game.Type("Runner", walker);
    uint8_t *list = game.Loose();
    Poke(list, 0, static_cast<const void *>(team));
    Poke(list, 8, int32_t{ 0x30 });
    Poke(list, 16, static_cast<const void *>(move));
    Poke(list, 24, int32_t{ 0x48 });
    Poke(walker, fake::kInterfacesAt, static_cast<const void *>(list));
    Poke(walker, fake::kInterfacesAt + 8, int32_t{ 2 });
    Poke(walker, fake::kInterfacesAt + 12, int32_t{ 2 });
    uint8_t *request = game.Make("XRequestMove", game.function, move);
    uint8_t *teamId = game.Make("GetGenericTeamId", game.function, team);
    uint8_t *location = game.Make("K2_GetActorLocation", game.function, actor);
    uint8_t *npc = game.Make("Runner_C", runner, nullptr);
    That(engine::Facet(npc, request) == npc + 0x48 && engine::Facet(npc, teamId) == npc + 0x30, "Facet picks the right offset from a two-entry interface table inherited from the super class");
    That(call::Fits(npc, move, request) && call::Fits(npc, team, teamId), "both interface functions fit the subclass");
    That(engine::Facet(npc, location) == npc && call::Fits(npc, actor, location), "an actor function keeps the object itself as the call target");
    uint8_t *stone = game.Type("Stone", game.object);
    uint8_t *rock = game.Make("Stone_C", stone, nullptr);
    That(engine::Facet(rock, request) == rock && !call::Fits(rock, move, request) && !call::Fits(rock, actor, location), "a class without the interface gets no facet and fits neither function");
    uint8_t *scripted = game.Type("BP_Scripted_C", pawn);
    game.Implements(scripted, move, 0);
    uint8_t *puppet = game.Make("BP_Scripted_C_1", scripted, nullptr);
    That(engine::Facet(puppet, request) == puppet && !call::Fits(puppet, move, request),
         "an interface entry at PointerOffset 0 (implemented in Blueprint, no native interface part) does not fit the native interface function, as UE's GetInterfaceAddress gives no address for it");

    uint8_t *manager = game.Type("SquadManager", game.object);
    uint8_t *special = game.Type("SquadManagerSpecial", manager);
    game.Make("Default__SquadManager", manager, nullptr);
    uint8_t *heir = game.Make("SquadManagerSpecial_C", special, nullptr);
    That(engine::Host(manager) == heir, "with no instance of the class itself a live subclass instance is the host, never the class default object");
    uint8_t *radio = game.Type("RadioManager", game.object);
    That(engine::Host(radio) == nullptr, "a class with no object has no host");
    uint8_t *tuned = game.Make("RadioManager_C", radio, nullptr);
    That(engine::Host(radio) == nullptr, "inside the retry window the host is not searched again");
    zsdk::detail::world::Jolt("memsafety shift", GetTickCount64());
    That(engine::Host(radio) == tuned, "a world shift clears the host cache, the next lookup finds the new host at once");

    zsdk::Flush();
    return check::Finish("memsafety");
}
