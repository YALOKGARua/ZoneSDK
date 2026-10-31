# ZoneSDK - ReShade add-on SDK for S.T.A.L.K.E.R. 2

ZoneSDK is a C++ source library for people who write ReShade add-ons for S.T.A.L.K.E.R. 2: Heart of Chornobyl. It is compiled into each add-on, so an add-on built with it is one `.addon64` file and needs nothing else at run time.

**Players do not need ZoneSDK.** If you only want to play with an add-on that was built with it, install that add-on and ReShade; this package is for mod authors.

Українською: [README-UA.md](README-UA.md).

## What you get

- A starter add-on (`release\1.0.2\template\ZoneTemplate`) that `new-addon.cmd` copies into a folder of your own, with a new name, tag, key and toast lane.
- Two sample add-ons to read: `samples\ZoneHelloHud` (a HUD panel fed by reflected game functions) and `samples\ZoneFieldWatch` (the safety model for raw memory reads).
- One-command tools: `doctor.cmd` checks your PC, `build.cmd` / `test.cmd` build and test, `fakehost.cmd` runs your add-on without the game, `build.cmd -Verify` checks a finished file, `pack.cmd` makes the zip for players.
- The safety model, built in:
  - game calls run only on the game thread, through the SDK's pump; the drawing side never calls the game;
  - every risky call carries a crash marker: a call that crashed once stays off until the player presses "Try again" in the add-on's Diagnostics block;
  - features that depend on exact game bytes are tied to known game builds and switch themselves off on a build the SDK does not know;
  - a structured exception (for example an access violation) in one of your ReShade-side hooks (frame, overlay, page, the GPU hooks), in a job or in the census hooks switches that part of the add-on off and writes one line to the log; that part stays off, in later game sessions too, until the player presses "Try again". In a game-thread hook it stops every game call for the rest of that session. C++ exceptions are still caught and logged by the inner handler;
  - settings and the log live in `%LOCALAPPDATA%\<add-on folder>`, texts come in Ukrainian and English.

A live example: the A-Life add-on Zone Pulse (`ZonePulse.addon64`) is built on an earlier version of ZoneSDK: https://www.nexusmods.com/stalker2heartofchornobyl/mods/3021

## Requirements

To build add-ons:

- Windows 10 or Windows 11, 64-bit.
- Visual Studio 2022 version 17.10 or newer, or Visual Studio 2026. The free Build Tools are enough. You need the workload "Desktop development with C++" (the MSVC x64 tools) and a Windows 11 SDK. [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) shows the install.
- Windows PowerShell 5.1, which every Windows 10 and 11 has. The `.cmd` launchers always use it.
- Python 3 is optional. `build.cmd -Verify` uses it to read the finished file when it is there and falls back to PowerShell when it is not.

To run add-ons in the game:

- S.T.A.L.K.E.R. 2: Heart of Chornobyl, **Steam version**. ZoneSDK 1.0.2 knows two game builds: `2bd3f115`, game version 2.1 "The Zone Lookout" (Steam build 26123434, out on 26 October 2026; its exe is dated 21 October 2026), and `61bc1e03`, game version 2.0.6 (Steam, exe dated 18 September 2026), which stays known. Other editions are not supported. On the Game Pass build an add-on loads, says "This game build is not supported" and changes nothing. On a game build ZoneSDK does not know, for example after a game update, features tied to a known build stay off until ZoneSDK knows the new one; see [docs/PATCH_DAY.md](docs/PATCH_DAY.md).
- ReShade 6.8 or newer, the download **with full add-on support** (add-on API 20), installed for the game with DirectX 12.

### Tested with

ZoneSDK 1.0.2 was tested with MSVC 14.44 (toolset 14.44.35207, Visual Studio 2022 Build Tools) and Windows SDK 10.0.26100.0, on Windows 11 with Windows PowerShell 5.1 and ReShade 6.8.0. Visual Studio 2026 and other MSVC versions in the supported range are accepted but were not tested. If a newer compiler warns about a ZoneSDK file, the build prints `sdk warnings N (compiler newer than tested)` and goes on; warnings in your own code still stop the build.

## How the toolchain is found

Every build looks for the C++ tools in this order and uses the first one that works:

1. `ZSDK_VCVARS`: the full path to a `vcvars64.bat`, or to your own `.bat` that sets up `cl`, `link`, `rc` and `dumpbin`.
2. `_toolchain\vcvars-winsdk.bat` in the release folder or in any folder above it, for a toolchain kept next to your SDK folders.
3. `vswhere`: the newest installed Visual Studio 2022 17.10+ or Visual Studio 2026 that has the MSVC x64 tools. Prerelease builds of Visual Studio are skipped.

`ZSDK_VCVARS_VER=14.44` asks for one MSVC toolset (it is passed to `vcvars64.bat` as `-vcvars_ver`). Each build prints the tools it used, for example `toolchain msvc 14.44.35207, windows sdk 10.0.26100.0`, and `doctor.cmd` shows the versions of `cl`, `link`, `rc` and `dumpbin`.

## Quick start in 5 commands

Unblock the zip first (see below), then unzip it into a folder for your mods; the zip holds one folder, `ZoneSDK-1.0.2`. Open Command Prompt or PowerShell in `ZoneSDK-1.0.2` and run:

```
.\doctor.cmd
.\new-addon.cmd -Name MyHud -Target ..\MyHud -Author "Your Name" -Tag YourNameMyHud
..\MyHud\build.cmd -Tests -Verify
.\fakehost.cmd -Addon ..\MyHud\bin\MyHud.addon64 -All
..\MyHud\pack.cmd
```

1. `doctor` checks PowerShell, the toolchain, Python, the game folder, ReShade and the game build. It only reads.
2. `new-addon` makes your own add-on `MyHud` next to the SDK folder, from the starter. Pick a tag nobody else uses: putting your author name in front is the easy way.
3. `build -Tests -Verify` builds `MyHud\bin\MyHud.addon64`, runs its test suites and the checks for a finished file.
4. `fakehost -All` loads the add-on into a fake game process and runs it through start, exit, kill and crash cases, without the game. It ends with `fake host modes: N checks, 0 failures, M skipped`.
5. `pack` writes `MyHud\dist\MyHud-0.1.0.zip` for players.

Each step is explained in [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md), including how to try the add-on in the game.

## Folder layout

```
ZoneSDK-1.0.2\
  README.md, README-UA.md                this file
  CHANGES.md, CHANGES-UA.md              what changed in each version
  LICENSE, NOTICE, THIRD-PARTY-NOTICES.txt
  doctor.cmd, new-addon.cmd, fakehost.cmd
  docs\                                  guides in English and Ukrainian
  samples\ZoneHelloHud\                  sample 1
  samples\ZoneFieldWatch\                sample 2
  release\1.0.2\                         ZoneSDK itself
    include\zsdk\  src\  extern\  data\  tests\  build\  tools\  api\
    template\ZoneTemplate\               the starter that new-addon copies
    MANIFEST.sha256                      the hash of every file in the release
    NOTICE-ADDON.txt                     goes into every add-on zip
```

Never edit anything under `release\1.0.2`: every build compares the release with its `MANIFEST.sha256` and stops if a file changed. The starter cannot be built in place either; `new-addon.cmd` makes your own copy.

### Where your add-on lives, and ZSDK_ROOT

Your add-on is a folder of its own, outside `release\`. Its `build\zsdk.lock` pins the ZoneSDK version and the release manifest it was made with, so it keeps building with exactly that release until you move it. The build finds the release in this order:

1. `ZSDK_ROOT`: the unpacked ZoneSDK folder, the one that holds `release\1.0.2`;
2. `home=` in `build\zsdk.lock`: a path relative to the add-on folder. `new-addon` writes it when the add-on cannot find the release by itself, for example `home=..\ZoneSDK-1.0.2`;
3. a walk up the parent folders of the add-on, looking for `release\1.0.2` or `_sdk\release\1.0.2`.

A release with another manifest is passed over. When nothing fits, the error lists every place it tried. `build.cmd -Where` prints the release it would use, without building.

If your add-on sits on another drive than the SDK, set `ZSDK_ROOT` before you build. In Command Prompt: `set "ZSDK_ROOT=<folder>\ZoneSDK-1.0.2"`; in PowerShell: `$env:ZSDK_ROOT = '<folder>\ZoneSDK-1.0.2'`. Both last until you close the window.

## Downloaded zip: Unblock-File and the execution policy

Windows marks files that come from the internet (Mark-of-the-Web). Remove the mark before you unzip: right-click the zip, choose Properties, tick **Unblock** and press OK. Or, in PowerShell, in the folder of the zip:

```
Unblock-File -LiteralPath .\ZoneSDK-1.0.2.zip
```

If you have already unzipped it, run this in PowerShell inside `ZoneSDK-1.0.2`:

```
Get-ChildItem -LiteralPath . -Recurse | Unblock-File
```

Use the `.cmd` launchers: `doctor.cmd`, `new-addon.cmd` and `fakehost.cmd` in the package folder, `build.cmd`, `test.cmd` and `pack.cmd` in each add-on folder. Each one starts Windows PowerShell with `-ExecutionPolicy Bypass` for its own script only, so it works with the default policy and with marked files, and it changes no setting. A `.ps1` started directly is refused by the default policy ("running scripts is disabled on this system"), and a marked one also by RemoteSigned ("is not digitally signed"). If you want to run the `.ps1` files yourself, unblock the files and allow local scripts for your user with `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`. When a Group Policy forces Restricted or AllSigned, nothing can bypass it; `doctor.cmd` tells you so.

Started by a double-click in Explorer, a launcher waits for a key at the end so you can read the output. Set `ZSDK_NOPAUSE=1` to turn that off.

## Several add-ons, different ZoneSDK versions

Every add-on carries its own copy of ZoneSDK inside its `.addon64`; there is no shared DLL. So add-ons built on different ZoneSDK versions run side by side in one game, and updating one add-on never touches another.

What must differ between add-ons is the **tag**. It names the add-on's one-instance mutex and its game-thread message. When two add-ons in one game share a tag, the second one silently does not load; it only writes `ZoneSDK: <name> does not load: the tag <tag> is already taken ...` to the Windows debug output, which a debug viewer such as DebugView shows. `new-addon.cmd` takes the tag from `-Tag` (or uses the name), refuses tags that ZoneSDK's own table, a sample or an add-on next to the target already uses, and recommends your author name in front. The settings folder, the key and the toast lane should differ too; `new-addon.cmd` picks a key and a lane that the table, the samples and the add-ons next to the target do not use, and warns that a player may still have another add-on on them. The identity test suite warns as well when an add-on in a folder next to yours has the same toast lane or default key.

## Release manifest

Each release folder is locked by its `MANIFEST.sha256`. The SHA-256 of `release\1.0.2\MANIFEST.sha256` in this package is:

```
FD712C93CCAA8B28E027E7F902A14837913968D9D9BB5ECC3DC0D8A12B537A78
```

To check it, run `Get-FileHash -LiteralPath .\release\1.0.2\MANIFEST.sha256` in PowerShell inside the package folder. Builds print the first 8 characters (`zsdk 1.0.2 manifest XXXXXXXX`), and every add-on writes the same `zsdk 1.0.2 manifest XXXXXXXX` into its log when it starts, so two copies of the same version can be told apart.

## Documentation

- [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md): from an empty PC to your add-on in the game, step by step, with the common errors.
- [docs/RULES.md](docs/RULES.md): what each thread may and may not do, the safety model, and what each test suite checks.
- [docs/API.md](docs/API.md): every public header in `include\zsdk`, with examples.
- [docs/VERSIONING.md](docs/VERSIONING.md): what is stable, what may change, and how versions are numbered.
- [docs/PATCH_DAY.md](docs/PATCH_DAY.md): what happens to add-ons after a game update and what authors can do.
- [CHANGES.md](CHANGES.md): the list of changes.
- [samples/ZoneHelloHud/README.md](samples/ZoneHelloHud/README.md) and [samples/ZoneFieldWatch/README.md](samples/ZoneFieldWatch/README.md): the samples, file by file.

Every page in this list also comes in Ukrainian, with `-UA` in the name; start from [README-UA.md](README-UA.md).

## Plans (roadmap)

What comes after 1.0.2, in this order. There are no dates; each step ships when it is ready and tested.

- **1.1**
  - Safe raw calls of native game functions through one shared gate, with a fault log and "Try again".
  - A shared book of game facts and readable descriptions of game classes, so a game update is fixed in one place.
  - A tool that finds the new addresses after a game update, and a full list of game functions.
- **1.2**
  - A black box: after a crash it shows which add-on was doing what, and a second crash at the same step switches that part off by itself.
  - Knowledge of game saves: data kept per save, and a journal for the player.
  - A new census of NPCs and mutants, shared by all add-ons.
  - A fake game kit for testing add-ons without the game.
- **1.3**
  - Objects and effects in the world without hitches, with safe removal, traces and attachment.
  - After a fault, the SDK gives back to the player the control, the camera and anything else an add-on was holding.
- **1.4 and later**
  - Shared, safe modules for seats, an own camera, teleport and NPC control, for vehicle-style mods.
  - Helpers for big add-ons: a JSON store, loading level pieces, navigation, the PDA map and game windows.

Later ideas, not planned yet: support for other editions of the game (Game Pass), and a patch-day helper that turns a new game exe into a ready build description for your add-on.

## Licence and credits

ZoneSDK is made by YALOKGAR and released under the MIT licence: see [LICENSE](LICENSE). You may use it in your own add-ons, change it and share it, as long as the licence text stays with it.

ZoneSDK builds against third-party headers that keep their own licences: the ReShade add-on API headers (BSD-3-Clause or MIT), Dear ImGui 1.92.5 (MIT) and the stb headers that come with it (MIT or public domain). See [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt) and `release\1.0.2\extern\LICENSES`.

Every add-on built with ZoneSDK contains code from ZoneSDK and those headers, so every add-on zip must carry `NOTICE-ADDON.txt`. `pack.cmd` adds it for you; if you pack by hand, copy `release\1.0.2\NOTICE-ADDON.txt` into your zip.

ZoneSDK is fan-made. It is not made, approved or endorsed by GSC Game World and is not affiliated with GSC Game World. S.T.A.L.K.E.R. 2: Heart of Chornobyl is a trademark of its owners and is named here only to say which game ZoneSDK works with. ZoneSDK contains no game files or game assets, only what an add-on needs to work together with the running game: short masked search patterns, addresses and byte checks for known game builds, and the names and parameter layouts of engine functions. See [NOTICE](NOTICE).

Bug reports and questions: on the ZoneSDK page on Nexus Mods.
