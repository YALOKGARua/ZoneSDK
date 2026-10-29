# When the game updates

A S.T.A.L.K.E.R. 2 update brings a new game exe. Code and data inside it move, so an add-on that reads the game's memory or calls its functions has to be careful on a build it was not made for. This page says what happens then, for players, for add-on authors and for ZoneSDK itself.

ZoneSDK 1.0.1 knows two game builds, both from Steam: `2bd3f115`, game version 2.1 "The Zone Lookout" (Steam build 26123434, released 26 October 2026, exe dated 21 October 2026), and `61bc1e03`, game version 2.0.6 (exe dated 18 September 2026). ZoneSDK supports the Steam version of the game only. On another store's build an add-on loads, shows "This game build is not supported, the add-on changes nothing" and makes no game calls.

**The promise.** On a game build it does not know, a ZoneSDK add-on switches the parts that depend on the exact build off by itself instead of crashing the game. A new ZoneSDK release that knows the new build follows when that is possible. There is no fixed date for it.

Ukrainian version: [PATCH_DAY-UA.md](PATCH_DAY-UA.md). See also [RULES.md](RULES.md) and [VERSIONING.md](VERSIONING.md).

## 1. What players see

After an update the add-on still loads, its tab opens and its settings work. ZoneSDK patches nothing in the game: it changes no game code, so nothing has to be undone. Then one of two things happens.

**ZoneSDK still finds the engine.** It looks for the engine by short masked patterns, not by fixed addresses, so this is the usual case. Then:
- features that use the game's own functions and fields by name (through the game's reflection) usually keep working; ZoneSDK checks each function against the live game first;
- features that depend on the exact bytes of the old exe switch themselves off; the add-on's log has a line such as `guard: field <name> is not usable on this build (guard set is not on), its read or write is refused`;
- the add-on's Diagnostics shows the game build as a hash with "other" instead of "known (2bd3f115)";
- ZoneSDK no longer sees the game's own "exit requested" signal; the log says `exit watcher off: ..., only WM_QUIT and the exit mark remain`. A normal quit is still told apart from a crash by the exit mark, so nothing is disabled by mistake. An add-on that saves something of its own at the moment you quit may skip that save;
- the first calls on the new build write their crash markers again, once each.

**ZoneSDK cannot find the engine.** Then every game call stays off for the session. The add-on shows the toast and the Diagnostics line "This game build is not supported, the add-on changes nothing", its phase reads "Does not work with this game version", and its log says `this game build is not supported: native bindings rejected`.

What players can do:
- Nothing is required. Check the add-on's page for an update made for the new game build.
- If the game crashes, the add-on switches off what was running at the crash at the next start, and Diagnostics lists it under "Disabled after a crash". Do not press "Спробувати ще / Try again" over and over on the new build: every press lets that call run once more, and it may crash again. Wait for an update of the add-on.
- To report a problem, press "Скопіювати звіт / Copy report" in the add-on's Diagnostics and paste the text into your report. It names the game build and the add-on and ZoneSDK versions, and hides your user name.
- To turn an add-on off, untick it on the Add-ons tab of ReShade, or remove its `.addon64` file from `Stalker2\Binaries\Win64` in the game folder.

### Choose when Steam updates the game

In Steam, open the game's Properties, then Updates, and set automatic updates to "Only update this game when I launch it". This does not stop the update: Steam still installs it when you start the game. It only keeps the update from arriving in the background, so you know when it happens and can check your add-ons' pages first.

## 2. What add-on authors do

**1. See whether ZoneSDK knows the build.** `doctor.cmd` reads the installed game and reports it, for example:

```
warn  game build 0a1b2c3d is unknown to ZoneSDK 1.0.1, which knows 2bd3f115, 61bc1e03: add-ons built on it start, but their guarded game features stay off until an update
```

For a game folder that Steam does not list, pass it: `.\doctor.cmd -Game "<game folder>"`. The add-on's own log also says it at every start: `exe: <sha256> unknown build, the verb check decides`.

**2. Forecast your guard sets.** `.\build.cmd -Tests -LiveExe` compares every guard set in `App::sets` with the installed game exe, and `.\build.cmd -Tests -Exe "<path to the new Stalker2-Win64-Shipping.exe>"` with any copy you name. Both only read the exe and never copy it. On a build your sets do not name, the `zsdk-guards` suite prints one line per set and does not fail:

```
guards: set myhud.core (needs bytes, image size and the exact build, build 61bc1e03): 1/1 guards match, image differs -> off, the image size is 0xB964000, the set wants 0xB963000
```

**3. Check your reflection features in game.** Functions and reflected fields usually survive an update. Start the game with the new build and read the add-on's log. Once the engine is ready, the line `verbs: <n>/<n> required, <n>/<n> optional, missing <names>` names every function of your table that is missing or no longer fits, and those stay off; a reflected field whose offset moved is named in a `guard: field <name> is +0x... in <class>.<property>, expected +0x..., its features are off` line. Such a field needs only the new offset from that line, or a second reflected field with it while the add-on still supports the old build.

**4. Bring a byte-guarded feature back.** Find the same code in the new exe: a pattern search for the old guard bytes, or a disassembler. Then add, next to the old set, a `guard::Build` for the new exe and a set and a field bound to it. List both sets in `App::sets` and use the field whose set is on:

```cpp
inline constexpr zsdk::guard::Build kNext{ .id = "0a1b2c3d", .sha256 = "0A1B2C3D...", .imageSize = 0xB964000u, .stamp = 0x6A1B2C3Du, .objectsRoot = 0, .objectsCount = 0 };

inline constexpr zsdk::guard::Guard kGuards61[] = { { .rva = 0x23CA0E4, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00" } };
inline constexpr zsdk::guard::Guard kGuardsNext[] = { { .rva = 0x23CB1F4, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00" } };

inline constexpr zsdk::guard::Set kCamera61{ "myhud.camera.61bc1e03", &zsdk::guard::k61bc1e03, kGuards61, 1, zsdk::guard::Need::KnownBuild };
inline constexpr zsdk::guard::Set kCameraNext{ "myhud.camera.0a1b2c3d", &kNext, kGuardsNext, 1, zsdk::guard::Need::KnownBuild };

inline constexpr zsdk::guard::Field kLocked61{ "Camera.LockedFOV", 0x2CC, kCamera61, 0, 8 };
inline constexpr zsdk::guard::Field kLockedNext{ "Camera.LockedFOV.next", 0x2CC, kCameraNext, 0, 8 };

constexpr const zsdk::guard::Set *kSets[] = { &kCamera61, &kCameraNext };

inline const zsdk::guard::Field &Locked()
{
    return zsdk::guard::Of(kCameraNext) == zsdk::guard::State::Ok ? kLockedNext : kLocked61;
}
```

- `.id` is the first 8 hex digits of the new exe's SHA-256 in lower case, `.sha256` all 64 in upper case. Get it with `powershell -NoProfile -Command "(Get-FileHash '<path to the exe>').Hash"`. The `guards: exe ...` line that `zsdk-guards` prints before the set lines gives the exe's image size and time stamp. `objectsRoot` and `objectsCount` are not used by guard sets.
- With `Need::KnownBuild` a set is on only on the exact exe it names, so the old set stays off on the new build and the new one off on the old build.
- Prove it: `.\build.cmd -Tests -Exe "<path to the new exe>"`. The exe is now the build of `kCameraNext`, so every guard of that set must match, or the suite fails. The old set comes out off with `the set is for build 61bc1e03 and the exe is build 0a1b2c3d`; `-RequireExe` does not count that as a miss, because the set is pinned to another build.
- When a ZoneSDK release already knows the new build, point the new set at its row (for example `&zsdk::guard::k2bd3f115`) instead of a `guard::Build` of your own.

**5. Release.** Raise your add-on's version, run `.\build.cmd -Tests -Verify` and `.\pack.cmd`, and say in the release notes which game build you tested.

**6. When the engine is not found.** If the log says `this game build is not supported: native bindings rejected`, no game call can work, whatever the add-on does: finding the engine is ZoneSDK's part. The same holds for the exit signal. Wait for a ZoneSDK release that knows the new build, move your add-on to it ([VERSIONING.md](VERSIONING.md#moving-an-add-on-to-a-newer-release)), and release again.

Never put a game exe, or any other game file, into your add-on or its repository.

## 3. What ZoneSDK does

When an update changes the game exe, the ZoneSDK maintainer checks the engine patterns and ZoneSDK's own guard sets against the new exe. When the new build can be supported, a patch release (1.0.1, 1.0.2 and so on) follows. It adds the new build: its `guard::Build` row, its `data\builds\<id>` folder, and the SDK's own guard sets for it. A patch release changes no API, so moving an add-on to it is the lock edit in [VERSIONING.md](VERSIONING.md#moving-an-add-on-to-a-newer-release).

Every add-on carries its own copy of ZoneSDK, so a new release helps players only after the author rebuilds and re-releases the add-on. Until then, the self-disable described above is what keeps the game running.

## 4. A real case: game update 2.1

On 26 October 2026 Steam installed game version 2.1 "The Zone Lookout" (Steam build 26123434). The game exe changed from build `61bc1e03` (version 2.0.6) to build `2bd3f115`. The update came before ZoneSDK 1.0.0 was released, so 1.0.0 itself took the steps of section 3, and the FieldWatch sample took the author's steps of section 2.

What ZoneSDK checked and changed:
- The four short patterns that find the engine's object array matched the new exe unchanged, each exactly once. They now lead to the object array at `0xA0D4990` instead of `0xA0E68D0`, so add-ons still find the engine.
- The 13 functions of the shared verb catalog are in the new exe with the same parameter layouts.
- The exit watcher's guard set no longer matched: `RequestEngineExit` and the game's exit flag had moved (the flag from `0x9FE3628` to `0x9FC9600`) with the same code around them. Without a new set, the log says `exit watcher off: guard set zsdk.exit 0/2 match, ...`, as in section 1.
- So 1.0.0 knows both builds: `guard::k2bd3f115` joined `guard::kBuilds`, `data\builds\2bd3f115` holds its `build.json` and function dump, and the exit watcher has one guard set per build, `zsdk.exit.61bc1e03` and `zsdk.exit.2bd3f115`. It uses the set that matches the running exe and logs `exit watcher on: GIsRequestingExit at exe+0x9FC9600, guard set zsdk.exit.2bd3f115 2/2 match` on the new build. Nothing was dropped: on `61bc1e03` everything works as before.

What the FieldWatch sample did (section 2, step 4):
- Its `Camera.LockedFOV` field is proven by a byte guard on `APlayerCameraManager::GetFOVAngle` at `0x23CA0E4`. A pattern search for the old guard bytes found the same 54 bytes once in the new exe, at `0x22B5CDE`, with `0x2CC` still at byte 8.
- The sample now has a set per build, `fieldwatch.camera.61bc1e03` and `fieldwatch.camera.2bd3f115`, with a `Camera.LockedFOV` field bound to each. Both sets are in `App::sets`, and the sample reads the field whose set is on. The new set points at `zsdk::guard::k2bd3f115`, because ZoneSDK 1.0.0 knows that build.
- `.\build.cmd -Tests -Exe` with each exe proves it. On `2bd3f115` the suite prints `guards: set fieldwatch.camera.2bd3f115 (needs bytes, image size and the exact build, build 2bd3f115): 1/1 guards match, image ok -> on`, and the old set comes out off for another build; on `61bc1e03` it is the other way round.
- On any other build both sets stay off, and the sample still logs `game build <id> is not known to this ZoneSDK; guard set fieldwatch.camera off`.

The reflected `Camera.DefaultFOV` needed no change. Whether every function still behaves the same in the running game is checked in game (step 3), not offline.
