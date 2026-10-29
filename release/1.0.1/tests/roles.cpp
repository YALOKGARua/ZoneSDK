#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "../src/detail/runtime.h"
#include "support/probe.h"

#include <string>

namespace
{
    using check::That;
    namespace verbs = zsdk::verbs;
    namespace common = zsdk::verbs::common;

    constexpr verbs::Spec kOwn[] = {
        { "Actor", "K2_GetActorLocation", true, { { "ReturnValue", common::kStruct, 0, 24 } }, verbs::Mark::Risky },
    };

    constexpr auto kJoined = verbs::Join(common::kWorld, kOwn);
    static_assert(common::Plays(kJoined, common::kFrameCount, common::kFrameCount) && common::Plays(kJoined, common::kPlayerController, common::kPlayerController));
    static_assert(!common::Plays(kOwn, 0, common::kPlayerController) && !common::Plays(kJoined, common::kWorldCount, common::kFrameCount) && !common::Plays(kJoined, -1, common::kFrameCount));
}

int main()
{
    That(common::Plays(probe::kSpecs, 0, common::kPlayerController) && common::Plays(probe::kSpecs, 4, common::kFrameCount) && !common::Plays(probe::kSpecs, 4, common::kPlayerPawn),
         "Plays tells the catalog verb of a role by owner and name, wherever the table keeps it");
    That(!common::Plays(probe::kSpecs, 6, common::kFrameCount) && !common::Plays(probe::kSpecs, -1, common::kFrameCount), "an index outside the table plays no role");
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path logFile = home / "roles.log";
    zsdk::log::Open(logFile, home / "roles.1.log");
    zsdk::App &app = probe::Mutable();
    app.world.pawn = 4;
    app.world.seconds = 99;
    app.world.frames = 5;
    That(zsdk::detail::world::Advance(), "a frame step runs with the misnamed roles");
    zsdk::Flush();
    const std::string log = scan::Body(logFile);
    That(log.find("world: WorldVerbs names verb 5 (Actor.K2_GetActorLocation) for KismetSystemLibrary.GetFrameCount, the role is off") != std::string::npos,
         "a frame role that names another add-on verb is switched off with one line, never fired with zeroed parameters");
    That(log.find("world: WorldVerbs names verb 4 (KismetSystemLibrary.GetFrameCount) for GameplayStatics.GetPlayerPawn, the role is off") != std::string::npos, "a catalog verb in the wrong role is off too");
    That(log.find("world: WorldVerbs names verb 99 (outside the table) for GameplayStatics.GetTimeSeconds, the role is off") != std::string::npos, "an index past the table is off");
    That(log.find("for GameplayStatics.GetPlayerController") == std::string::npos && log.find("for GameplayStatics.GetPlayerCameraManager") == std::string::npos && log.find("for GameplayStatics.IsGamePaused") == std::string::npos,
         "roles that name their catalog verb, and a role set to -1, say nothing");
    That(scan::Count(log, "the role is off") == 3, "one line per misnamed role, the check runs once");
    That(zsdk::detail::world::Advance(), "a second frame step");
    zsdk::Flush();
    That(scan::Count(scan::Body(logFile), "the role is off") == 3, "the second step says nothing more");
    return check::Finish("roles");
}
