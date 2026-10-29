# Getting started with ZoneSDK

This guide takes you from a PC with no build tools to your own add-on running in S.T.A.L.K.E.R. 2. Follow it from top to bottom the first time. Every command in steps 2 to 8 was run on a fresh copy of the package.

Українською: [GETTING_STARTED-UA.md](GETTING_STARTED-UA.md).

The examples use an add-on called `MyHud` and this layout:

```
Mods\
  ZoneSDK-1.0.1\     the unpacked package
  MyHud\             your add-on, made in step 4
```

Commands work the same in Command Prompt and in PowerShell. Type them as shown, with the `.\` in front.

## 1. Install the build tools

You need Visual Studio 2022 (17.10 or newer) or Visual Studio 2026 with the C++ tools. The free **Build Tools** are enough; a full Visual Studio (Community, Professional) works too.

1. Download "Build Tools for Visual Studio" from the Visual Studio downloads page of Microsoft (visualstudio.microsoft.com, section "Tools for Visual Studio").
2. In the installer, tick the workload **Desktop development with C++**. Keep the parts it selects: the MSVC x64 build tools and a Windows 11 SDK.
3. Install, then open a new command window.

With winget the same install for Visual Studio 2022 is one line:

```
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

ZoneSDK finds the tools by itself through `vswhere`. If you keep them elsewhere, point `ZSDK_VCVARS` at their `vcvars64.bat` (see "How the toolchain is found" in the [README](../README.md)).

Optional:

- Python 3 (from python.org). `build.cmd -Verify` uses it to read the finished file and falls back to PowerShell without it. `ZSDK_PYTHON` can point at a `python.exe`, and `ZSDK_PYTHON=none` turns Python off. The build looks for `python.exe` on `PATH`, not for the `py` launcher: if doctor shows only `py.exe`, set `ZSDK_PYTHON` to the full path of your `python.exe`.

For the test in the game (step 9) you also need the Steam version of the game and ReShade 6.8 or newer, the download **with full add-on support**. Run the ReShade setup, pick `Stalker2-Win64-Shipping.exe` in `<game folder>\Stalker2\Binaries\Win64` and choose Direct3D 10/11/12.

## 2. Unzip and unblock

Windows marks downloaded files (Mark-of-the-Web). Remove the mark from the zip **before** you unzip it: right-click the zip, Properties, tick **Unblock**, OK. Or in PowerShell, in the folder of the zip:

```
Unblock-File -LiteralPath .\ZoneSDK-1.0.1.zip
```

Then unzip it into `Mods`. The zip holds one folder, `ZoneSDK-1.0.1`.

Already unzipped without unblocking? Run this in PowerShell inside `ZoneSDK-1.0.1`:

```
Get-ChildItem -LiteralPath . -Recurse | Unblock-File
```

The `.cmd` launchers work even on marked files, because each one runs its script with `-ExecutionPolicy Bypass`. Unblocking matters when you run a `.ps1` yourself.

## 3. Check the PC: doctor

Open a command window in `Mods\ZoneSDK-1.0.1` and run:

```
.\doctor.cmd
```

It only reads, and prints one line per check: `ok`, `warn`, `info` or `FAIL`. On a healthy PC the main lines look like this:

```
ok    toolchain wrapper ...\VC\Auxiliary\Build\vcvars64.bat
ok    cl.exe 14.44.35228.0 at ...
ok    Windows SDK 10.0.26100.0, MSVC tools 14.44.35207
ok    game folder ... (Steam libraryfolders.vdf, 2 libraries)
ok    ReShade 6.8.0 (dxgi.dll), add-on loader present; ...
ok    game build 2bd3f115 (2026-10-21) is known to ZoneSDK 1.0.1
doctor: 0 problems, 1 warnings
```

The date in brackets is the build date written into the game exe, not the day Steam released the update. It exits with code 1 when any line is `FAIL`. A `warn` about the execution policy only means that a plain `.ps1` would not run; the launchers still do. Useful options:

- `.\doctor.cmd -Game "<game folder>"` when Steam does not find the game (the folder that holds `Stalker2`);
- `.\doctor.cmd -Addon MyHud` once your add-on was in the game: is it switched off in `ReShade.ini`, and did ReShade load it last time;
- `.\doctor.cmd -Selftest` also runs ZoneSDK's own scaffold self-test (check-scaffold) inside the package. It takes about seven minutes and is not needed for normal work. On the way it prints `FAIL` lines from cases that are meant to fail; the result is the line `ok    self-test: check-scaffold passed inside the release`. It works in a new folder under `%TEMP%` and deletes it after a pass; after a failure it keeps the folder and names it.

The game itself is not needed to build add-ons: without it, doctor warns and goes on. When the game is there but ReShade is not installed for it yet, doctor shows one `FAIL` line for ReShade and exits with code 1; building, the tests and the fake host work without ReShade. Every launcher (`doctor`, `new-addon`, `build`, `test`, `pack`, `fakehost`) prints its options with `-Help` or `-?`.

## 4. Make your add-on: new-addon

```
.\new-addon.cmd -Name MyHud -Target ..\MyHud -Author "Your Name" -Tag YourNameMyHud
```

This copies the starter add-on into `Mods\MyHud`, renames everything inside, and prints what it chose:

```
created MyHud in ...\Mods\MyHud
  define MYHUD, namespace zmyhud, header src\myhud.h
  tag YourNameMyHud, toast lane 0.72 (picked: ...), key ALT+H (picked: ...)
  ZoneSDK 1.0.1 manifest XXXXXXXX from ...\release\1.0.1 pinned in build\zsdk.lock, home=..\ZoneSDK-1.0.1
```

The options:

| Option | What it does |
| --- | --- |
| `-Name` | The add-on name: 2 to 40 Latin letters and digits, starting with a letter. A `Zone` prefix is only a habit, not a rule. Names with `template` in them are refused. |
| `-Target` | The new folder. It must not exist yet and must lie outside `release\`. |
| `-Author` | Your name as it goes into the file's version resource: ReShade shows it, and `pack` needs it. Latin letters, digits, spaces and `. , _ & ( ) ' + -`; a name in Cyrillic is refused, so write it in Latin letters here and keep the Cyrillic spelling for your mod page. |
| `-Tag` | The tag, see below. Default: the name. |
| `-Key` | The default key, for example `ALT+K` or `CTRL+SHIFT+F7`. Default: a free ALT chord from ZoneSDK's table. Numpad keys are refused, because many keyboards have none. |
| `-Lane` | Where on the screen the add-on's toasts appear, as a fraction of the screen height from 0 to 0.95; 0 means no toasts. Default: a free lane from ZoneSDK's table. |
| `-Define` | The prefix of the add-on's C++ macros. Default: the name in capitals without `Zone`, here `MYHUD`. |
| `-Release` | Another unpacked release folder to pin. Default: the release that `new-addon` runs from. |

**The tag must be unique** among all ReShade add-ons a player has. It names the add-on's one-instance mutex and its game-thread message. If two add-ons in one game share a tag, the second one silently does not load. Put your author name in front, as in `YourNameMyHud`. `new-addon` refuses a name or tag that ZoneSDK's own table or one of the samples already uses, and a tag of an add-on in a folder next to the target. It picks a key and a lane that the table, the samples and the add-ons next to the target do not use, and warns that an add-on it cannot see may still use them. A `-Key` or `-Lane` you give that one of them already uses gets a warning too.

What you get in `Mods\MyHud`: `src\` (your code), `tests\` (your tests and `tests\zsdk.cfg`), `build\make.ps1`, `build\pack.ps1`, `build\zsdk.lock`, `dist\README-EN.txt` and `dist\README-UA.txt` (the readmes for players), the launchers `build.cmd`, `test.cmd` and `pack.cmd`, and a `.gitignore` that keeps the build outputs and the zips out of git. At the end `new-addon` prints its next steps. Two of them differ from the steps below: its step 3 is a single fake host run without `-All` (step 6 here runs every mode), and its step 4 is a plain `.\build.cmd -Verify`, which leaves a build that `pack` warns about; use `.\build.cmd -Tests -Verify` (step 7) instead.

`build\zsdk.lock` pins ZoneSDK 1.0.1 and its manifest. `home=..\ZoneSDK-1.0.1` in it tells the build where the release is. If you move the add-on to another drive, set `ZSDK_ROOT` to the ZoneSDK folder instead (see the [README](../README.md)).

The starter allows comments in your code. If you want the test suite to forbid them, add the line `comments=forbid` to `tests\zsdk.cfg`.

## 5. Build and test

```
cd ..\MyHud
.\build.cmd -Tests
```

`.\test.cmd` does the same. The first build takes about a minute. The end of a good build:

```
tests passed 9 failed 0 checks 1081, 2 test warnings
sdk warnings 0
warnings 0
```

- The add-on is `bin\MyHud.addon64`; the full log is `build\last-build.log`.
- The 2 test warnings are normal for a new add-on: the guards suite had no game exe to compare with, and your add-on is not in ZoneSDK's table, so its own row is used.
- Warnings in your own code stop the build: 0 are allowed.
- When a test fails, the build removes `bin\MyHud.addon64` and its pdb, so a failed build can be neither packed nor installed. A passing test build writes `bin\MyHud.tested`, and `pack` warns when the binary it packs was not built with passing tests. Every build removes that mark first, so a later build without `-Tests` (for example a plain `.\build.cmd -Verify`) makes `pack` warn: build with `-Tests` last before you pack.
- `.\build.cmd -Tests -Exe "<game folder>\Stalker2\Binaries\Win64\Stalker2-Win64-Shipping.exe"` or `-LiveExe` (the installed game, found through Steam) lets the guards suite compare your byte guards with a real exe. It only reads the exe.
- `.\build.cmd -Config Debug` writes a Debug build to `bin\Debug\`. Never ship it: `pack` takes only `bin\MyHud.addon64`, and `-Verify` refuses a Debug build.
- `.\build.cmd -Where` shows which ZoneSDK release the add-on uses, without building: the release folder, how it was found, the version, the manifest and the lock file.
- Two builds of one add-on never run at the same time: the second one waits for the first (it holds `build\.zsdk-build.lock`), up to 30 minutes.

## 6. Run it without the game: fakehost

```
..\ZoneSDK-1.0.1\fakehost.cmd -Addon bin\MyHud.addon64 -All
```

The fake host is a small program named like the game exe. It loads your `.addon64` the way ReShade does, presents frames, opens the overlay and the tab, and ends the process in different ways: a normal exit, a kill, an exit while drawing, an exit from another thread, a crash and a quit message. It checks the add-on after every run. The first time it builds itself, which takes a few seconds more. A full run takes about three minutes and ends with:

```
fake host modes: 116 checks, 0 failures, 35 skipped
```

The 35 skipped checks (the groups `gpu`, `job`, `seh`, `report` and `quit`) need ZoneSDK's own fault test add-on, which breaks things on purpose and is not in the package; `-Expect` switches them on for it, and every other add-on skips them. The exit code is the number of failures. Each run's log, settings and result are in `%LOCALAPPDATA%\ZoneSDK\1.0.1\fakehost\harness\MyHud`, with `summary.txt` listing every check.

The fake host never starts while the game or another fake host is running. Without `-All` it does one plain run; `new-addon` prints that form with `-Label` and `-Out`, if you only want one log to look at.

## 7. Check the finished file: -Verify

```
.\build.cmd -Tests -Verify
```

This builds again, runs the tests and then checks the file that players would get:

- it exports exactly `NAME` and `DESCRIPTION`, as ReShade expects;
- it has a version resource and is not a Debug build (no Debug flag, no Debug runtime);
- its bytes name none of your folders: not the add-on folder, not the ZoneSDK folder, not your user folder or a path with your Windows user name in it, and no other absolute drive path;
- one fake host run: no ImGui call outside the overlay, a second copy is refused, the tab is drawn.

It ends with `verify: passed (fake host, exports, version resource, byte scan)`. `.\build.cmd -Verify` without `-Tests` checks the same, but leaves a build that `pack` warns about (step 5).

## 8. Pack the zip for players

1. Set your version in `src\version.h` (`MYHUD_VERSION_MAJOR`, `_MINOR`, `_PATCH`) and build again with `.\build.cmd -Tests`. For a version you ship, build with `.\build.cmd -KeepSymbols`: it makes a tested Release build and keeps the binary, its pdb, the lock, the build log and a zip of your sources in `dist\<version>\symbols`, never overwritten. Every later build replaces `bin\MyHud.pdb`, and a debugger needs the pdb of the shipped binary to turn a log line's `MyHud.addon64+0x...` into a function and a source line.
2. Edit `dist\README-EN.txt` and `dist\README-UA.txt`: these go into the zip for players. `pack` fills `{{VERSION}}`, `{{AUTHOR}}`, `{{GAME}}` and `{{ZSDK}}` for you.
3. Pack:

```
.\pack.cmd
```

It writes `dist\MyHud-0.1.0.zip` and `dist\MyHud-0.1.0-SHA256.txt`. The zip holds:

```
NOTICE-ADDON.txt
README-EN.txt
README-UA.txt
SHA256.txt
Stalker2/Binaries/Win64/MyHud.addon64
```

Players (and mod managers) unpack it into the game folder. Packing the same build twice gives the same zip. `pack` takes only the Release build, refuses a file with the Debug flag, and warns when a source file is newer than the build or when the build did not pass its tests; `.\pack.cmd -Build` builds and tests first.

`NOTICE-ADDON.txt` carries the licences of ZoneSDK and the headers inside your add-on. If you pack by hand, copy `release\1.0.1\NOTICE-ADDON.txt` from the ZoneSDK folder into your zip.

## 9. Try it in the game

ZoneSDK 1.0.1 has no installer, so you copy the file by hand:

1. Close the game.
2. Copy `bin\MyHud.addon64` into `<game folder>\Stalker2\Binaries\Win64`, next to ReShade's `dxgi.dll`. Or unpack your zip into the game folder, the one that holds `Stalker2`.
3. Start the game and press **Home** to open ReShade. The add-on has its own tab with its name, `MyHud`. The **Add-ons** tab lists it too and can switch it off.
4. Load a save and press the add-on's key (`ALT+H` in this example; the tab shows it and lets you change it). The starter shows where you stand.

The tab ends with the **Diagnostics** block: the game build (known, other, or not the Steam build), how many game functions were found, the calls that were switched off after a crash, **Try again** and **Copy report**. Use it first when something does not work.

## 10. Logs and settings

Each add-on keeps its files in `%LOCALAPPDATA%\<its folder>`, for `MyHud` in `%LOCALAPPDATA%\MyHud`:

| File | What is in it |
| --- | --- |
| `myhud.ini` | The settings, the key, and the safety lists (calls switched off after a crash). |
| `myhud.log` | The log. Each game session adds its lines at the end; the first lines of a session name the add-on, the language and `zsdk 1.0.1 manifest XXXXXXXX`. |
| `myhud.1.log` | The older part of the log, moved aside when the log grows past 4 MB at a game start (8 MB while the game runs). |
| `myhud.exit` | The clean-exit mark, written when the game closes normally. |
| `report.txt` | Written by **Copy report**, which also puts the same text on the clipboard. User names and personal paths are masked. |

ReShade writes its own `ReShade.log` and `ReShade.ini` in `<game folder>\Stalker2\Binaries\Win64`.

The language is `auto` at first: a game set to Ukrainian gives Ukrainian texts, every other game language gives English. The tab can switch it to `uk` or `en`.

## 11. Uninstall

- The add-on: delete `<game folder>\Stalker2\Binaries\Win64\MyHud.addon64`. To remove its settings and logs too, delete `%LOCALAPPDATA%\MyHud`.
- ZoneSDK: delete the `ZoneSDK-1.0.1` folder. `%LOCALAPPDATA%\ZoneSDK` holds only the fake host and test work folders; you can delete it at any time, it is made again when needed. `doctor -Selftest` deletes its `%TEMP%\zsdk-scaffold-...` folder after a pass; a folder it kept after a failure can be deleted by hand.

## 12. Common errors

| What you see | What to do |
| --- | --- |
| `... cannot be loaded because running scripts is disabled on this system` | You started a `.ps1` directly. Use the `.cmd` launcher instead, or allow local scripts with `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`. |
| `... is not digitally signed. You cannot run this script on the current system.` | The file still carries the Mark-of-the-Web. Unblock it (step 2) or use the `.cmd` launcher. |
| doctor: `A Group Policy forces Restricted` (or AllSigned) | Nothing in ZoneSDK can bypass a Group Policy; ask the administrator of the PC. |
| `no C++ toolchain was found. ...` | Install the Build Tools (step 1), or set `ZSDK_VCVARS` to your `vcvars64.bat`. The message lists every place it looked. |
| `the MSVC x64 build tools are missing: ...` | In the Visual Studio Installer, Modify, tick **Desktop development with C++**. |
| `the Windows SDK is missing: rc.exe does not resolve ...` | In the Visual Studio Installer, Modify, Individual components, tick a Windows 11 SDK (10.0.26100 was tested). |
| `sdk warnings N (compiler newer than tested)` | Not an error: your compiler is newer than the one ZoneSDK was tested with and warns about an SDK file. The build goes on. |
| `the build has N warnings in add-on code, 0 are allowed` | Fix the warnings in your own code; they are in `build\last-build.log`. |
| `tests failed: N; the binary is removed, so neither pack nor an install can ship it: ...` | The `FAIL` lines above it name the suite and the check; a failing `zsdk-*` suite also points to its entry in [RULES.md, section 10](RULES.md#10-the-generic-test-suites). Fix your code and build again with `.\build.cmd -Tests`. |
| `ZoneSDK 1.0.1 with manifest XXXXXXXX was not found for ... Tried: ...` | The add-on cannot find its release. Set `ZSDK_ROOT` to the ZoneSDK folder, or put `home=<path relative to the add-on>` into `build\zsdk.lock`. If a release is found but "is another build of 1.0.1", you unpacked a different package than the one the add-on was made with: use that package. |
| `the ZoneSDK snapshot was edited: <file>` (or `... edited: missing <file>`, `... edited: unlisted <file>`) | A file under `release\1.0.1` was changed, removed or added. Unzip the package again; never edit the release. |
| `this add-on folder lies inside the ZoneSDK release ...` | The starter is never built in place. Make your own add-on with `new-addon.cmd`. |
| `the target exists and is never overwritten` | `new-addon` needs a new folder: pick another `-Target`. |
| `the tag ... is taken in the ZoneSDK registry table` | Pick another `-Tag`, best with your author name in front. |
| doctor: `ReShade ... is older than 6.8`, `has no add-on loader` or `ReShade is not installed` | Install ReShade 6.8 or newer, the download **with full add-on support**, for the game with DirectX 12. An older ReShade does not load ZoneSDK add-ons. |
| The add-on's tab does not appear in the game | Run `.\doctor.cmd -Addon MyHud`. `MyHud is disabled in ReShade.ini`: switch it on in the Add-ons tab of ReShade. `ReShade.ini AddonPath=`: ReShade looks for add-ons in another folder. Otherwise check the ReShade version (above) and the tag (below). |
| Two add-ons, one of them never loads | They share a tag. A debug viewer such as DebugView shows `ZoneSDK: <name> does not load: the tag <tag> is already taken ...`. Give one of them another tag and rebuild it. |
| The tab shows `Parts of MyHud were switched off after an error: <parts>. The log has the details.` | A structured exception (for example an access violation) hit the tab page, the add-on's start or the SDK's core work. The add-on's own tab is then replaced by this text, the log path, **Try again** and **Copy report**. A fault in another part (for example `frame` or a GPU hook) leaves the tab as it is, and Diagnostics lists `present.<lane>` under `Disabled after a crash`. Either way the log has one line `structured exception 0x... at <module>+0x... in the <lane> lane, ...`; with the pdb of that build (step 8) a debugger shows the function and line of a `MyHud.addon64+0x...` offset. Fix the code and rebuild, then press Try again. See [RULES.md](RULES.md#55-when-something-faults). |
| Diagnostics shows `Game build: <12 hex digits> (other)`, or doctor says `game build ... is unknown` | The game was updated, or this is not one of the Steam builds ZoneSDK knows (`doctor.cmd` lists them). The add-on starts, but features tied to a known build stay off. See [PATCH_DAY.md](PATCH_DAY.md). |
| The toast `This game build is not supported, the add-on changes nothing` | The game exe is not the Steam build (for example Game Pass), or a game update changed the exe so much that ZoneSDK cannot find the engine in it. ZoneSDK 1.0.1 supports only the Steam version; for an update see [PATCH_DAY.md](PATCH_DAY.md). |
| fakehost: `a process named Stalker2-Win64-Shipping is running (the game or another fake host); the fake host does not start next to it, ...` | Close the game first. Another fake host run (for example `-Verify` in a second window) counts too, because the fake host has the same name: wait until it ends. |
| fakehost: one check fails once, then passes | Some checks measure timing. On a busy PC, close heavy programs and run it again. A failure that repeats is a real problem. |
| `verify: FAIL the binary contains an absolute path: ...` | Your code puts a folder of your PC into the file, for example through `__FILE__` or a literal path. Remove it. |
| `refused: ... is a Debug build ...` | Build and pack a Release build: `.\build.cmd -Tests`, then `.\pack.cmd`. |
| pack: `WARNING: ... was not built with passing tests (build.cmd -Tests or test.cmd): pack.cmd -Build tests it first` | The last build of `bin\MyHud.addon64` ran without `-Tests`. Build with `.\build.cmd -Tests` (or use `.\pack.cmd -Build`) and pack again. |
| pack: `dist\README-EN.txt needs {{AUTHOR}}, but it is empty ...` | The add-on was made without `-Author`. Set `MYHUD_AUTHOR` in `src\version.h` and build again. |

## Next

- [RULES.md](RULES.md): what each thread may do, how the safety model works, what each test suite checks.
- [API.md](API.md): every header, with examples.
- [VERSIONING.md](VERSIONING.md): what stays stable and how to move an add-on to a newer ZoneSDK.
- [PATCH_DAY.md](PATCH_DAY.md): what happens after a game update.
- The samples in `samples\ZoneHelloHud` and `samples\ZoneFieldWatch`, each with a README that walks through its files: [ZoneHelloHud](../samples/ZoneHelloHud/README.md), [ZoneFieldWatch](../samples/ZoneFieldWatch/README.md).
