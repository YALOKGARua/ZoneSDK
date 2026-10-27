# ZoneSDK changes

Ukrainian: [CHANGES-UA.md](CHANGES-UA.md)

## 1.0.0

First public release.

ZoneSDK is the SDK that ReShade add-ons for S.T.A.L.K.E.R. 2 are built on. Every add-on compiles its own copy of it, so players install only the add-on and never ZoneSDK itself. This list describes what 1.0.0 contains.

### Requirements

- S.T.A.L.K.E.R. 2 on Steam (`Stalker2-Win64-Shipping.exe`). ZoneSDK knows two game builds: `2bd3f115`, game version 2.1 "The Zone Lookout" (Steam build 26123434, released 26 October 2026, exe dated 21 October 2026), and `61bc1e03`, game version 2.0.6 (exe dated 18 September 2026). On another store's build (for example Game Pass), or when ZoneSDK cannot find the engine, an add-on still loads, keeps every game call off and says that the build is not supported. On a Steam build it does not know, only the features tied to a known build stay off.
- ReShade 6.8 or newer with full add-on support (add-on API 20).
- Windows 10 or 11 (x64) with Windows PowerShell 5.1.
- Visual Studio 2022 17.10 or newer, or Visual Studio 2026. The Build Tools with the C++ workload and a Windows SDK are enough. Tested with MSVC 14.44.35207 and Windows SDK 10.0.26100.0.
- Python 3 is optional.

### Starting an add-on

- `new-addon.cmd -Name <Name> -Target <folder>` copies the starter add-on into a folder of yours and renames everything in it. A name is 2 to 40 ASCII letters and digits; the `Zone` prefix is only a suggestion. Every identifier made from the name (define, namespace, file names) is checked, and a name or tag that contains `template`, that the ZoneSDK registry table already holds or that belongs to a sample is refused, as is a tag of an add-on in a folder next to the target.
- `-Author` fills the version resource that ReShade shows in its add-on list; it takes Latin letters, and a Cyrillic name is refused with a hint. `-Tag` (the name by default), `-Lane` and `-Key` get defaults that the registry table, the samples and the add-ons next to the target do not use, never a numpad key; a given lane or key that one of them uses gets a warning. The tag must be unique: two add-ons with the same tag never load together.
- The new add-on gets `build\zsdk.lock`, which pins it to this release by version and manifest hash, and a `.gitignore` for the build outputs, and the tool prints the next steps (`.\test.cmd`, the fake host run, `.\build.cmd -Verify`, `.\pack.cmd`).
- In the add-on folder, `build.cmd` builds, `test.cmd` builds and runs the tests, and `pack.cmd` makes the zip for players. Every launcher prints its options with `-Help` or `-?`, and `new-addon.cmd` and `fakehost.cmd` also do so when they are started without arguments.
- The starter add-on (ZoneTemplate) has a language choice (auto, uk, en; auto by default), its own registry row with a tag, a toast lane and a key, the shared world verb catalog, designated initializers, the ZoneSDK default texts for Diagnostics, keys and phases, its own tests and player readmes in `dist\`.
- Two samples, sources only, each with tests and readmes in English and Ukrainian:
  - ZoneHelloHud draws a corner panel with the player's position, compass heading and speed. It uses reflected UFunction calls only, and shows the frame hook with `thread::Ask`, publishing under a mutex, overlay painting, an ini schema, a key field, toasts, Diagnostics and its own section in Copy report.
  - ZoneFieldWatch shows the memory safety model: a field proven by reflection, a field proven by byte guards with `Need::KnownBuild` in one guard set per known game build (it reads through the set that is on), an unproven field listed in `tests\unproven-fields.txt`, crash markers, a verdict job and a Diagnostics block with the build verdicts.

### Building

- The `.cmd` launchers run the PowerShell scripts with `-ExecutionPolicy Bypass`, so they work under any execution policy and on files from a downloaded zip that carry Mark-of-the-Web. At the package root: `new-addon.cmd`, `doctor.cmd`, `fakehost.cmd`. In every add-on: `build.cmd`, `test.cmd`, `pack.cmd`. Started by a double click in Explorer they wait for a key; `ZSDK_NOPAUSE` turns that off.
- The toolchain is found in this order: `ZSDK_VCVARS` (a vcvars batch file), a `_toolchain\vcvars-winsdk.bat` in a parent folder, then vswhere, which picks Visual Studio 2022 17.10+ or 2026 with the C++ tools and never a prerelease. `ZSDK_VCVARS_VER` picks an MSVC toolset (`14.xx`). The build log names the Visual Studio, MSVC and Windows SDK versions. When a tool is missing, the error names it (no `rc.exe` means no Windows SDK) and says what to install.
- Only the variables that vcvars adds or changes are imported. `TEMP`, the user profile and other folders with letters outside the ANSI code page stay intact, and add-ons in such folders build and test.
- An add-on finds its ZoneSDK release through `ZSDK_ROOT` (the unzipped ZoneSDK folder), then `home=` in `build\zsdk.lock`, then the parent folders. A release whose manifest differs from the lock is skipped, and a miss lists every place that was tried. `build.cmd -Where` shows the release that would be used, without building.
- The boot log reads `zsdk 1.0.0 manifest <8 hex digits>`, so two trees with the same version number can be told apart.
- A release folder is never written to. The SDK self-test and the fake host put their outputs in `%LOCALAPPDATA%\ZoneSDK\<version>`, and building an add-on folder that lies inside a release is refused.
- Add-on code must build with 0 warnings. A warning that a newer compiler raises inside ZoneSDK's own code does not stop the build; it is counted as `sdk warnings N (compiler newer than tested)`.
- Two builds of the same sources give the same `.addon64`. A compiler without `/experimental:deterministic` still builds, with one log line.
- `/GL` and `/LTCG` are refused, because whole-program objects hide the symbols that the present/overlay check reads. That check fails the build even without `-Tests`.
- A build holds `build\.zsdk-build.lock`, so a second build of the same add-on waits for the first (`-WaitMinutes`).
- `build.cmd -Config Debug` writes `bin\Debug\<Name>.addon64` with its pdb and the `VS_FF_DEBUG` flag, and leaves the Release file in `bin\` alone.
- `build.cmd -Verify` checks the finished file: a fake host run, exactly the exports `NAME` and `DESCRIPTION`, a version resource, no Debug build (by flag, configuration or Debug runtime), and no build folder, user profile, path with the Windows user name in it or other absolute drive path inside the binary. When the fake host run fails, the error carries the fake host's own reason. It works without Python, and `ZSDK_PYTHON=none` forces that.
- When the tests of a `-Tests` build fail, the build removes the binary and its pdb, so neither `pack.cmd` nor an install can ship it. A passing test build writes `bin\<Name>.tested`, and `pack.cmd` warns when the binary was not built with passing tests.
- The link writes no `.lib` or `.exp` file next to the add-on (`/NOIMPLIB /NOEXP`).
- `build.cmd -Where` prints the release folder, how it was found, the version, the manifest and the lock file as a list.
- `build.cmd -KeepSymbols` makes a tested Release build and keeps the add-on, its pdb, `zsdk.lock`, the build log and a source zip in `dist\<version>\symbols`, so a player's crash dump can be read later. Kept symbols are never overwritten.
- `pack.cmd` writes `dist\<Name>-<version>.zip` for players and mod managers: `Stalker2/Binaries/Win64/<Name>.addon64`, `README-EN.txt` and `README-UA.txt` filled from `dist\`, `NOTICE-ADDON.txt` and `SHA256.txt`. The zip is identical byte for byte on every run and never takes a Debug build, a pdb or a log.
- `build.cmd -Tests -Exe <path to the game exe>` compares the guard sets with that exe, which is only read, never copied. `-LiveExe` finds the installed game through Steam, and `-RequireExe` turns every miss into a failure. A set pinned to another known build than the given exe is reported and never required, so an add-on can keep one set per game build.

### Checks in every add-on build

`test.cmd` runs the add-on's own tests and the generic `zsdk-*` suites that ship with ZoneSDK. Skipped checks and other warnings print visible `warning:` lines, and the summary counts them as `N test warnings`. A failed generic suite adds a line that points to its part of RULES.md, section 10, where every message is explained.

- `zsdk-hardrules`: banned words match whole identifiers, so a longer name that only contains one passes. Own threads, timers and thread-pool callbacks (`CreateThread`, `std::thread`, `std::jthread`, `std::async`, `_beginthread`, `QueueUserWorkItem`, `CreateTimerQueueTimer`, `SetTimer` and others) fail with a pointer to `job::Every`.
- `zsdk-identity`: tag, wake and mutex clashes with the add-ons next to yours, and a warning for their toast lanes and default keys. By default it reads sibling folders that carry `build\zsdk.lock`; `ZSDK_WORKSPACE` or `workspace=` in `tests\zsdk.cfg` points it at any folder. The scan is bounded. It fails an add-on without a settings schema (`App::schema`), and checks `registry=` and `define=` of `tests\zsdk.cfg` against the identity and the build.
- `zsdk-present`: present units never reach ImGui or the overlay, and every hook is a named function of the right kind; a failure names where the function is defined (`src/entry.cpp`, `src/overlay` or nowhere).
- `zsdk-specs`: the verb table against every function dump in `data\builds`, and every `WorldVerbs` role against the shared catalog. Without a dump it is skipped with a warning.
- `zsdk-style`: drive paths, deploy words in `build\make.ps1`, banned game calls, and raw tick differences (a warning). The no-comments rule is optional: `comments=forbid` in `tests\zsdk.cfg` turns it on.
- `zsdk-texts`: every row in Ukrainian and English, no Russian text and no Russian language code in any spelling, the same slots in both languages, glyphs the overlay can draw, and a fresh ini that gives the language `auto`.
- `zsdk-guards`: every guard set listed in `App::sets`, and the ZoneSDK sets, byte by byte against the exe given by `ZSDK_EXE`, `-Exe` or `-LiveExe`. On an unknown exe it prints an on/off forecast and does not fail. A guard set that is declared but missing from `App::sets` is a warning, and a failure under `-RequireExe`. Declared sets are found by their literal name, as the first value in the braces or as a designated `.name`.
- `zsdk-offsets`: no raw layout constants outside `guard::Field`, and every unproven field has its reason in `tests\unproven-fields.txt`.

### Tools

- `doctor.cmd` checks this PC and changes nothing: PowerShell and its execution policy, Mark-of-the-Web on the ZoneSDK files, the toolchain with its versions, Python, the game folder found through Steam (or `-Game <folder>`), ReShade 6.8+ with the add-on loader, add-ons disabled in `ReShade.ini`, and whether this release knows the game build. It tells a running fake host apart from the game. `doctor.cmd -Selftest` also runs the scaffold check from the unzipped package, in a `%TEMP%` folder that it deletes after a pass.
- `fakehost.cmd -Addon <file>` loads the add-on in a small host that stands in for the game and ReShade, so it runs with no game at all. Modes: `free`, `exit`, `kill`, `live`, `worker`, `crash` and `quit`; `-All` runs the whole set and fails on any miss. A plain `-All` gives `116 checks, 0 failures, 35 skipped` for an add-on that passes: the skipped checks need ZoneSDK's own fault test add-on (`-Expect gpu,job,seh,report,quit`). It refuses to run while the game or another fake host runs, and builds itself again when its own sources changed.
- `tools\check-scaffold.ps1` checks new-addon, building, testing and packing end to end.

### At run time: safety

- Crash fence for add-on callbacks. A structured exception, such as an access violation, in an add-on callback, or in the SDK's own work in the same ReShade event, no longer reaches ReShade or the game. Every callback runs in a lane of its own: ZoneSDK switches the lane off for the session, logs one line with the code, the module and offset, and the lane, and writes the log to disk at once; a call through a null pointer names the place it came from. The ledger key `present.<lane>` stays disabled in later sessions until the player presses «Спробувати ще / Try again», in Diagnostics or on the SDK's own notice that stands in for the tab while the tab's lane is off. That notice names the add-on and the parts that are off in the player's language, and shows the log path, Try again and Copy report. The frame and toasts lanes go off together. A fault in the boot or core lane stops the whole add-on except its cleanup and save at the end; when it happened in the running session, Try again brings the add-on back with the next game start. A GPU or device hook that comes before the first present opens the data folder first, so the keys of an earlier session hold there too, and a fault in it stays disabled. A thread with a stack of 1 MB or more gets a 32 KB stack guarantee when it first enters the fence, so a stack overflow there is fenced as well. C++ exceptions are caught and logged by the inner handler, at most once a minute per lane, and the callback stays on. A structured exception in a game-thread tick is now logged with its code, module and offset too.
- Jobs, the census hooks (`hooks.census`, `hooks.complete`; ledger key `census.hook`), `hooks.quit` and `hooks.stop` run inside fences of their own. The SDK worker survives a stack overflow in a job or a census hook. A job that has not returned after 1 s is logged once.
- All units of an add-on, overlay units included, build with `/EHa`, so a lock that add-on code holds is released when the fence stops a structured exception.
- An add-on without a settings schema (`App::schema`) stays off and says so on the Windows debug output, because ZoneSDK opens no data folder, settings or log for it.
- `shell::Ready()` turns true only after `hooks.boot` has returned, and stays false after a fault in the boot.
- Game exit. ZoneSDK reads the game's own exit flag `GIsRequestingExit` and still listens for WM_QUIT. Each known build has its own guard set for the reader (`zsdk.exit.2bd3f115`, `zsdk.exit.61bc1e03`); the reader uses the set that matches the running exe and switches itself off when no set matches. On the first signal ZoneSDK stops engine calls and the census, runs `hooks.quit` once (on the game thread or on the SDK worker, never beside `hooks.step`), closes the open crash windows, logs `game exit requested` and writes the log out.
- Exit mark. When the game exits normally, ZoneSDK writes a small mark file next to the add-on's ini, and the next start logs `ended cleanly (exit mark found)`. Without the mark, crash markers that were still set count as a crash. An ini written by an add-on on an earlier ZoneSDK (no `session=` line) is not judged once: its markers are cleared and nothing is disabled.
- At process exit ZoneSDK waits for nothing and only tries its locks, so a lock held by a thread that Windows has already ended cannot hang the exit. `shell::Exiting()` tells `hooks.stop` that the process is exiting.
- Proofs follow the add-on build. `[safety]` records the add-on build (`addon=`); after an update the proven keys are dropped once, so first calls carry their crash markers again.
- Objects that are being destroyed. `ue::Doomed` reads the object's own flags and its object array item (garbage, unreachable). With `Policy::verifiedOnly`, calls refuse a doomed target and doomed object, class or interface parameters. A host is never a doomed object or the class default object of a class that is not a function library, a missing host is refused as `host not ready: <key>` so the add-on can try again, the host cache is cleared on every world change, and census hosts come only from a sweep of the current world (wait reason `wait.consoles`).
- `Call::Set` refuses to write a borrowed string into an out, in-out or return `FString` parameter.
- `Bind` accepts a function of an interface that the object implements and calls it on the interface.
- A `WorldVerbs` role whose index does not name its catalog verb is switched off with one log line and never called.
- The patterns that find the engine's object array are short masked windows of 32 bytes. A good boot logs `native bindings: ready; 2 addresses, 4 of 4 patterns matched`.
- Other store builds: an add-on loads on any `Stalker2-*-Shipping.exe`, but only the Steam build is bound. On another build ZoneSDK scans nothing, keeps game calls off and shows "this game build is not supported" as a toast and in the tab.
- Until the exe hash is known, a guard set counts as pending, not as off: its fields wait, and the line "not usable on this build" appears only for a set that is really off. `guard::Ready(set)` tells whether a set has a verdict, and a set missing from `App::sets` is logged at most once a minute.
- Game update 2.1 (26 October 2026). The new exe `2bd3f115` is a known build in this release, and `61bc1e03` stays known. The engine patterns found it unchanged; the release adds its `guard::k2bd3f115` row in `guard::kBuilds`, its `data\builds\2bd3f115` folder and the exit reader's guard set for it, as [docs/PATCH_DAY.md](docs/PATCH_DAY.md) describes.

### At run time: game thread and SDK worker

- `job::Every(name, periodMs, run)` registers up to 8 background jobs from `hooks.boot`, each with a period of 100 ms or more. They run one after another on the SDK worker on every game build, also when game calls are off. A job that throws is logged (at most once a minute) and keeps running. A job that faults is switched off for the session and its ledger key `job.<name>` is disabled until Try again. A run longer than 1 s switches the job off for the session. `job::Stopping()` tells a long job to return. Game calls are refused in a job; hand results to the game thread with `thread::Ask`.
- The SDK worker is one loop from boot to stop with a 100 ms tick. Binding, the function book and the census are steps inside it, and it keeps ticking on builds ZoneSDK does not support. It is named `<add-on> census`, which crash dumps show.
- The heavy part of engine wiring (the memory survey and the function book) runs on the SDK worker instead of the game thread, and wiring gives up after 10 failed attempts with one log line.
- `Phase::Lost`: frames are presented, but the game thread has not ticked for 2 s outside the overlay, menus and loading. Diagnostics shows the pump counters.
- `world::Hold()` called off the game thread returns a copy instead of the live grip.
- The OS cursor counts as a menu only after it has been visible for 400 ms with no Alt, Ctrl or Shift held, so Alt chords work in the world while the inventory still counts as a menu.

### At run time: language, texts, logs and the registry

- The language `auto` is the default and follows the game: the text language in the game settings, then the game's language in Steam, then the Windows language. Ukrainian gives UA, every other language gives EN. There is no Russian option; a stored `ru` or an unknown code becomes `auto`.
- ZoneSDK ships UA and EN default rows for Diagnostics, key prompts, phases, yes/no, the "not supported" line, the notice about parts switched off after an error and the names of those parts (`lane.*`), so an add-on writes only the rows it changes, and `tab::Diagnostics(o)` needs no texts.
- The key field notes "The game uses this key too" only for a key without ALT that the game binds on its own.
- `lang::Format` reads the slots `{0}` to `{99}`.
- `ini::Visitor::Text` keeps the case of a value; `Word` lowercases only when it has a list of choices.
- Logs never show the user name or a full user path: the data folder is logged as `%LOCALAPPDATA%\<Folder>`. `log::Masked` and `log::Shown` do the same for add-on text.
- «Скопіювати звіт / Copy report» in Diagnostics copies a bug report to the clipboard and saves it as `report.txt` in the add-on's data folder: the add-on and ZoneSDK versions, the game exe build, the ReShade version, the add-ons next to the exe, the safety lists and the last 80 log lines, with user paths masked. `Hooks::report` adds the add-on's own section.
- The registry table lists the released add-ons and the template. An add-on that is not in it carries its own `App::row`. Without any row its toasts use the spare lane 0.75 and the log says so; lane 0 means no toasts. When an add-on with a tag that is already taken tries to load, it is refused and names the tag through `OutputDebugString` (DebugView shows it).

### New in the API

- `zsdk::paths`: the game folder, `Content\Paks\~mods`, the game's `Saved` folder and logs, the game settings file, the Steam manifest and the save folders.
- `zsdk/verbs/common.h`: the shared verb catalog with the six world verbs, the screen checks (cinematic, static dialog, PDA, backpack, interaction) and shared console commands, plus `verbs::Join` and `verbs::Index`. `WorldVerbs{}` points at the catalog.
- `ue::Lend` (an `FString` parameter view of a `std::wstring`), `ue::CopyText`, `ue::NameText`, `Call::CopyText`.
- `world::OwningWorld` gives the persistent world of an object; `world::WorldOf` gives its outer (cell) world.
- GPU hooks `pipelineInit`, `pipelineBind`, `pipelineDestroy` and `deviceDestroy` for the ReShade pipeline and device events. ZoneSDK registers an event only when its hook is set.
- `zsdk::Age(now, at)` for tick differences.
- `Policy::watchKept`: how many crash windows stay watched at once, 1 to 32 (8 by default).
- `thread::Count`, `thread::PumpAlive` and `thread::LastPumpAt`; `shell::Exiting`; `exe::File`, `exe::Name`, `exe::Game` and `exe::Steam`; `guard::Ready` and `guard::Own`; `census::Fresh`, `census::Host` and `census::Consoles`; `ledger::CleanStart`, `Stuck`, `Closed`, `Current`, `Last`, `WatchLimit` and `Addon`; `ini::ExitPath`; `lang::Detect` and `lang::Defaults`; `report::Text` and `report::Save`; `tab::Report`.

### API stability

- Semantic versioning of the source API. There is no binary ABI: each add-on compiles its own copy of ZoneSDK.
- `api\1.0.0.txt` in the release lists every public declaration with its tag: Stable; Provisional (registry, job, census and the screen checks of `verbs/common.h`, which may change in a minor release with a note here); Legacy (the old `Policy` knobs `marks`, `knock`, `wire`, `gone`, `spelling`, `widen` and `census` with their enums, which may be removed in 2.0); Internal (not for add-ons).
- Releases never change after they are cut and are pinned by their manifest. Add-ons built on different ZoneSDK versions run side by side.

### Licence and game data

- ZoneSDK is under the MIT licence. `THIRD-PARTY-NOTICES.txt` and `extern\LICENSES` cover the ReShade headers, Dear ImGui and stb.
- `NOTICE`: ZoneSDK holds no game files or assets, only what interoperability needs: short masked search patterns, short byte checks and addresses of identified builds, and function, class and property names and layouts in `data\builds\2bd3f115` and `data\builds\61bc1e03`. It is fan-made and not affiliated with GSC Game World.
- `NOTICE-ADDON.txt` lists every notice that an add-on zip needs. `pack.cmd` puts it into the zip; if you pack by hand, include it.
