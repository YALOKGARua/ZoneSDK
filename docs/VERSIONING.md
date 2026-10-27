# ZoneSDK versioning

This page says what a ZoneSDK version number promises, what counts as the public API, and how an add-on moves to a newer release.

Ukrainian version: [VERSIONING-UA.md](VERSIONING-UA.md). See also [API.md](API.md), [RULES.md](RULES.md), [PATCH_DAY.md](PATCH_DAY.md) and [CHANGES.md](../CHANGES.md).

## 1. Version numbers

ZoneSDK uses semantic versioning, `major.minor.patch`, for its **source API**.

There is no binary interface between add-ons and ZoneSDK. Every add-on compiles its own copy of the ZoneSDK sources from the release its `build\zsdk.lock` pins, and that copy lives inside its `.addon64`. So:

- add-ons built on different ZoneSDK versions run side by side in one game;
- a new ZoneSDK release changes nothing in an add-on that is already built: it takes effect when the author rebuilds and re-releases the add-on;
- add-ons share no state through ZoneSDK. Each one has its own tag, its own game-thread message, its own one-instance mutex and its own data folder.

| release | what it may contain |
|---|---|
| **patch** (1.0.0 to 1.0.1) | bug fixes, and new game build rows so the SDK knows a new game exe: a `guard::Build` in `guard::kBuilds`, its `data\builds\<id>` folder and the SDK's own guard sets for it. No new features |
| **minor** (1.0 to 1.1) | additions: new headers, functions, types and constants; fields appended at the end of a struct; enum values added before `Count`; new test rules, as warnings first |
| **major** (1.x to 2.0) | breaking changes, announced ahead |

Game update 2.1 (build `2bd3f115`, released on Steam 26 October 2026) came before 1.0.0 was released, so 1.0.0 already knows it beside `61bc1e03`. When a later game update can be supported, its build comes in a patch release of this kind ([PATCH_DAY.md](PATCH_DAY.md)).

## 2. What is public

The promise covers:

- the headers in `include/zsdk`, except the internal names listed below;
- the parameters of `Invoke-ZsdkBuild` in `build\zsdk.psm1`, which every add-on's `build\make.ps1` calls: `-Root`, `-Lock`, `-Name`, `-Define`, `-Config`, `-Tests`, `-Deterministic`, `-Pure`, `-Legacy`, `-Libs`, `-Includes`, `-Defines`, `-CompilerFlags`, `-LinkFlags`, `-WaitMinutes`, `-Exe`, `-LiveExe`, `-RequireExe`, `-Verify`;
- the formats of `build\zsdk.lock` (`version=`, `manifest=`, `home=`) and `tests\zsdk.cfg` (the keys in [RULES.md, section 10](RULES.md#zsdkcfg));
- the data folder: `%LOCALAPPDATA%\<Identity::folder>` holds the ini, the log, the previous log, the exit mark `<ini name>.exit` and `report.txt`. The ini sections `[safety]` and `[proven]` belong to the SDK; sections and keys it does not know survive every rewrite;
- the names one add-on can see from another: the tag, the game-thread message `<tag>.GameThread.1` and the mutex `Local\<tag>.Addon.<pid>`.

Not public: everything under `src`, every `detail` namespace, the internal names below, the log texts and the exact Diagnostics layout. The test headers in `include/zsdk/test` that add-on tests use (`check.h`, `scan.h`, `addon.h`) are public.

### Structs and enums

- A struct of the public API only ever gets new fields **at the end**. Fields are never removed, reordered or retyped within a major version.
- An enum only ever gets new values **before `Count`** (or at the end when it has no `Count`). Values are never removed or renumbered.
- So write designated initializers for every struct you fill, and let defaults cover the rest:

```cpp
.row = zsdk::registry::Row{ .id = "myhud", .tag = "AuthorMyHud", .lane = 0.66f, .keys = "ALT+H" },
```

- A `guard::Set` needs a literal name, either designated, `guard::Set kCoreSet{ .name = "myhud.core", ... }`, or as the first value in its braces, because the `zsdk-guards` test suite finds declared sets by that string ([RULES.md, section 6](RULES.md#appsets-list-every-guard-set)).

### Stability tags

Every public name carries one tag:

| tag | promise |
|---|---|
| **Stable** | everything not tagged otherwise; follows the rules above |
| **Provisional** | may change or go away in a minor release, always with a note in CHANGES. In 1.0: `zsdk::registry`, `zsdk::job`, `zsdk::census` and the screen checks of `zsdk/verbs/common.h` (`common::kScreen` and its `Screen` values) |
| **Legacy** | kept for add-ons written before ZoneSDK; may be removed in 2.0. In 1.0: the `Policy` knobs `marks`, `knock` (with `knockMs` and `idleMs`), `wire` (with `wireMs`), `gone`, `spelling`, `widen` and `census`, and their enums. A new add-on keeps the defaults |
| **Internal** | not for add-ons, no promise: every `detail` namespace, `zsdk/test/ledger_raw.h`, `zsdk/test/hardrules.h`, `ini::*Raw`, `ledger::Calm`, `log::Hushed`, `log::Due`, `thread::Booked`, `thread::Rejected` |

The tags live in `release\<version>\tools\api\tags.txt`. Every release also carries `release\<version>\api\<version>.txt`, the list of its public declarations, one per line, with the tag in the last column. Internal names are left out of it.

## 3. How the promise is kept

The release tool generates the API list of a new release and compares it with the previous release of the same major version. It refuses the release when a minor or patch release removes a public name, changes a signature or a type, removes, reorders or changes a struct field, or removes, reorders or renumbers enum values. A change to a Provisional name passes in a minor release with a note, and is refused in a patch.

You can run the same comparison on two releases yourself, from the ZoneSDK folder:

```
powershell -NoProfile -ExecutionPolicy Bypass -File release\1.1.0\tools\api\surface.ps1 -Compare release\1.0.0\api\1.0.0.txt -With release\1.1.0\api\1.1.0.txt -Kind minor
```

It prints every added name, every note and every refused change, then `api compare (minor): N added, N notes, N refused`; with a refused change it ends with an error.

Two more promises:

- **New test rules start as warnings.** A new rule of the generic test suites arrives in a minor release as a warning, so an add-on that built before still builds.
- **Removals are announced one minor release ahead.** CHANGES names a Provisional name before it changes or goes away, and a name that 2.0 will remove before 2.0.

## 4. Releases never change

- A release folder `release\<version>` is never edited after it is cut. `MANIFEST.sha256` lists the SHA-256 of every file, and every build checks the folder against it and stops when a file was changed, added or removed (the files of an unzipped package are not marked read-only). A fix is a new release.
- `build\zsdk.lock` pins one release by its version and by the SHA-256 of its `MANIFEST.sha256`. The build looks for a release folder whose manifest has exactly that hash and refuses every other copy of the same version number.
- The `zsdk-hardrules` suite checks the pin on every test run.

### Moving an add-on to a newer release

1. Unzip the new ZoneSDK package. Its release is in `ZoneSDK-<version>\release\<version>`.
2. Read [CHANGES.md](../CHANGES.md) for the versions in between.
3. Get the hash of the new manifest, from the new ZoneSDK folder:

   ```
   powershell -NoProfile -Command "(Get-FileHash 'release\1.0.1\MANIFEST.sha256').Hash"
   ```

4. In your add-on's `build\zsdk.lock`, set `version=` to the new version and `manifest=` to that hash. If the lock has a `home=` line, point it at the new ZoneSDK folder, relative to your add-on folder; without `home=`, set `ZSDK_ROOT` to that folder or keep the add-on inside it.
5. Check where the build now finds ZoneSDK: `.\build.cmd -Where` prints the release folder and how it was found, without building.
6. Run `.\build.cmd -Tests -Verify`, test in game, and release the add-on with a new version of its own.

Within one major version that is all it takes, unless you use a Provisional name that changed; CHANGES says so. The SDK's own tests and the API comparison above keep a minor or patch release from breaking an add-on that builds on the release before.
