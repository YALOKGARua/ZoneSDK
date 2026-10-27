# ZoneSDK API reference

This is the reference for the C++ API of ZoneSDK 1.0.0: one section for every header in `include/zsdk`. Each section says what the header is for, which thread may call it, its calls and types, a short example and its stability tag.

Українською: [API-UA.md](API-UA.md).

Read [GETTING_STARTED.md](GETTING_STARTED.md) first to create and build an add-on, and [RULES.md](RULES.md) for the rules the generic test suites enforce. [VERSIONING.md](VERSIONING.md) explains what the stability tags promise, and [PATCH_DAY.md](PATCH_DAY.md) what happens after a game update.

## How to read this reference

### Where the code goes

An add-on built from the template has three kinds of source files. The build compiles them differently, and the headers check that each file includes only what it may use.

| File | Starts with | Built as | May use |
|---|---|---|---|
| `src/*.cpp` and subfolders, except `src/overlay` and `src/entry.cpp` | `#include <zsdk/present.h>` | present units, with `ZSDK_PRESENT_UNIT` and `IMGUI_DISABLE` | everything in `zsdk/zsdk.h`, no ImGui, no `zsdk/overlay.h` or `zsdk/paint.h` |
| `src/overlay/*.cpp` | `#include <zsdk/overlay.h>` | overlay units | ImGui, `zsdk::tab`, `zsdk::paint` and everything in `zsdk/zsdk.h` |
| `src/entry.cpp` | `#include <zsdk/shell.h>` | overlay batch | `ZSDK_ADDON`, `DllMain` and the `zsdk::Hooks` table |
| `tests/*.cpp` | `#include <zsdk/present.h>` and the `zsdk/test` headers | one test program each, with `ZSDK_TESTS` | the add-on's present units, `check`, `scan` and `addon` |

The SDK sources are compiled into every add-on from a pinned release, so there is no binary interface between add-ons: each add-on has its own copy of ZoneSDK, and add-ons built on different ZoneSDK versions run side by side.

ZoneSDK 1.0.0 knows two Steam builds of S.T.A.L.K.E.R. 2: the exe build `2bd3f115` (game version 2.1, Steam build 26123434) and the exe build `61bc1e03` (game version 2.0.6). It needs ReShade 6.8 or newer with full add-on support (ReShade API 20). On other store builds the add-on loads but stays off and says so. After a game update, features that use the game's reflection usually keep working and byte-guarded features switch themselves off ([PATCH_DAY.md](PATCH_DAY.md)).

### Thread names

ZoneSDK code runs on several threads. Each section names the thread that may call it, with these words:

| Word | Thread | Hooks that run there |
|---|---|---|
| **game** | the game thread, inside the SDK pump | `refresh`, `prime`, `advance`, `step`, `hot`, `publish`, `turn`, `order`, `orderName` |
| **present** | the render thread, in ReShade's present event | `boot`, `frame`, `opened` |
| **overlay** | the render thread, inside ReShade's overlay event (ImGui frame) | `overlay`, `page`, `words`, `tone`, `report` |
| **worker** | the SDK census thread of the add-on | `census`, `complete`, the `job::Every` runs; `quit` may run here |
| **D3D** | any ReShade device thread, often in parallel | `pipelineInit`, `pipelineBind`, `pipelineDestroy`, `deviceDestroy` |
| **loader** | `DllMain`, under the Windows loader lock | `stop` |
| **any** | safe from every thread above | internally locked or pure functions |

Only the game thread may call the game: building a `zsdk::Call`, `zsdk::Fire` and the `zsdk::mem` writes refuse on every other thread. At the process exit (`hooks.stop` with `shell::Exiting()` true) the other threads are gone; [RULES.md](RULES.md#4-leaving-the-game-hooksquit-and-hooksstop) lists what is allowed there.

### Stability tags

Every section has a **Stability** line, and names with another tag are listed on their own line. The tags come from `tools/api/tags.txt`, which the release tool uses to write the API surface file:

- **Stable**: kept through all 1.x releases; fields are only appended, enum values only added before `Count`.
- **Provisional**: may change or go in a minor release, with a note in [CHANGES.md](../CHANGES.md).
- **Legacy**: kept for add-ons written before the SDK, may be removed in 2.0; new add-ons keep the defaults.
- **Internal**: used by the SDK and its tests; may change in any release; add-ons do not call it.

### The examples

The examples are cut from the template (`template/ZoneTemplate`) and the samples (`samples/ZoneHelloHud`, `samples/ZoneFieldWatch`). Names such as `Settings`, `kSpecs`, `kLocation`, `kLocateKey`, `Order` and `core::Snapshot` are the template's own. `tools/api/doccheck.ps1 -Release <release folder>` compiles every example of this page inside a scratch copy of the template and runs its tests, so the examples build with the release they describe.

## `zsdk/app.h`

The description of the add-on: who it is, which game functions it calls, its texts, settings, toast lane and guard sets. The add-on defines `zsdk::app::Get()` once, in a present unit, and returns one static `zsdk::App`. The SDK reads it from every thread and never changes it.

**Thread:** any. The `App` is built once, on first use, and stays constant.

**Stability:** Stable

| `App` field | What it holds |
|---|---|
| `identity` | an `Identity`: names and files of the add-on |
| `policy` | a `Policy` (see `zsdk/policy.h`); the template keeps `zsdk::Policy{}` |
| `specs` | the verb table, a `std::span` of `verbs::Spec`; `verbs::Prime` checks it against the game at prime, and `Call(table, verb)` accepts only this table |
| `world` | a `WorldVerbs`: where the six world verbs sit in `specs` |
| `census` | a `CensusSpec`: the class names the census keeps |
| `rows` | the add-on's language rows (`lang::Row`); the SDK default rows fill every id the add-on does not have |
| `schema` | the settings schema, an `ini::Schema` from `ini::Make`; ZoneSDK 1.0.0 needs one: without a schema the add-on stays off, gets no data folder, settings or log, and writes one line to the Windows debug output (`ZoneSDK: <name> stays off: the add-on has no settings schema ...`); the generic `identity` suite fails such an add-on |
| `row` | the add-on's own `registry::Row`: id, tag, toast lane and default keys |
| `sets` | every `guard::Set` the add-on declares |

`App::sets` must list every guard set of the add-on:

```text
constexpr const zsdk::guard::Set *kSets[] = { &kCoreSet };
a.sets = kSets;
```

The SDK checks the listed sets at prime, with one log line each. The generic `guards` suite compares only listed sets with the game exe; a set it finds in the sources but not in `sets` is a warning, and a failure under `make.ps1 -RequireExe`. At run time `guard::Of` logs at most once a minute that a set is not in `App::sets`.

| `Identity` field | What it is for |
|---|---|
| `name` | the add-on name; it titles the add-on's tab in ReShade and the log lines, and must equal the name the build uses |
| `tag` | a unique ASCII tag; it names the game-thread wake message and the one-instance mutex, so two add-ons with one tag cannot both load |
| `description` | the text ReShade shows for the add-on |
| `version` | the add-on version, in the boot log line, the report and `ledger::Addon()` |
| `folder` | the data folder under `%LOCALAPPDATA%` |
| `ini`, `log`, `rolled` | file names in the data folder: settings, log, the previous log |
| `registry` | the registry id; `app::Row()` falls back to the SDK table row of this id |

`WakeName(identity)` gives the wake message name `<tag>.GameThread.1`, and `MutexShape(identity)` the mutex name pattern `Local\<tag>.Addon.%lu` (filled with the process id). When the mutex exists already, `DllMain` returns FALSE and writes one `OutputDebugString` line: the second add-on with that tag does not load.

| `WorldVerbs` field | Role | Default |
|---|---|---|
| `controller` | `GameplayStatics.GetPlayerController` | `verbs::common::kPlayerController` |
| `pawn` | `GameplayStatics.GetPlayerPawn` | `kPlayerPawn` |
| `camera` | `GameplayStatics.GetPlayerCameraManager` | `kCameraManager` |
| `seconds` | `GameplayStatics.GetTimeSeconds` | `kTimeSeconds` |
| `paused` | `GameplayStatics.IsGamePaused` | `kGamePaused` |
| `frames` | `KismetSystemLibrary.GetFrameCount` | `kFrameCount` |
| `map` | the name of the open-world map | `"WorldMap_WP"` |

Each role is an index into `App::specs`. The defaults are the catalog indices, so a table that starts with `common::kWorld` (see `zsdk/verbs/common.h`) needs no `WorldVerbs` of its own. This changed in 1.0.0: in 0.1.x `WorldVerbs{}` meant "no world verbs" (all -1); now it means the catalog indices. Set a role to -1 to switch it off. At the first world step the SDK checks every role: an index that does not name its catalog verb by owner and name switches that role off with one log line and is never called, and the generic `specs` suite fails such a role at build time.

| `CensusSpec` field | What it holds |
|---|---|
| `statics`, `staticCount` | class names whose class default objects host the static calls, for example `common::kStatics` |
| `hosts`, `hostCount` | class names of live objects that host calls, for example `common::kConsoleHost` |

Each list holds at most `census::kSlots` (16) names. `App::schema` points to an `ini::Schema`; `app.h` only declares `zsdk::ini::Schema`, so include `zsdk/ini.h` (or `zsdk/zsdk.h`) to build one.

| Call | What it does |
|---|---|
| `app::Get()` | the add-on's `App`; written by the add-on |
| `app::Row()` | the add-on's own `row` when `row.id` and `row.tag` are set, otherwise the SDK table row of `identity.registry`, otherwise `nullptr` |
| `app::Lane()` | the toast lane: the row's lane, 0 (no toasts) when the row's lane is 0 or 1 and above, and `registry::kSpareLane` with one log line when there is no row |
| `WakeName(identity)` | the wake message name |
| `MutexShape(identity)` | the mutex name pattern |

The template's `src/app.cpp` (present unit):

<!-- doccheck: present -->
```cpp
const zsdk::App &Make()
{
    static const zsdk::App app{
        .identity = zsdk::Identity{ .name = "ZoneTemplate", .tag = "ZoneTemplate", .description = TEMPLATE_DESCRIPTION, .version = kVersion, .folder = L"ZoneTemplate", .ini = L"template.ini", .log = L"template.log", .rolled = L"template.1.log", .registry = "template" },
        .specs = kSpecs,
        .census = zsdk::CensusSpec{ .statics = common::kStatics, .staticCount = 2 },
        .rows = Rows(),
        .schema = &Schema(),
        .row = zsdk::registry::Row{ .id = "template", .tag = "ZoneTemplate", .lane = 0.54f, .keys = "ALT+L" },
    };
    return app;
}
```

The template's `zsdk::app::Get()` returns this `Make()`. Use designated initializers as above: fields are only ever appended, so the code keeps building on later 1.x releases.

## `zsdk/base.h`

Clocks and small helpers used everywhere.

**Thread:** any.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Now()` | milliseconds since Windows started (`GetTickCount64`); the `now` that hooks receive |
| `Ms()` | a high-resolution clock in milliseconds, as a `double`, for timing |
| `Age(now, at)` | `now - at`, or 0 when `at` is later than `now`; use it for every tick difference |
| `Addr(thing)` | a pointer as `uintptr_t`, for `mem::Read` and `mem::Write` |
| `kNaN` | a quiet NaN `double` |

The generic `style` suite warns about a raw `now - x` in add-on sources, because a stamp taken on another thread can be later than `now`. The FieldWatch sample's step:

<!-- doccheck: present body -->
```cpp
static uint64_t readAt = 0;
if (zsdk::Age(now, readAt) < 250) return;
readAt = now;
const double from = zsdk::Ms();
const double fov = zsdk::exe::Known() ? 90.0 : zsdk::kNaN;
zsdk::Note(2, "fieldwatch: read %.0f at %llu in %.3f ms", fov, static_cast<unsigned long long>(zsdk::Now()), zsdk::Ms() - from);
```

## `zsdk/call.h`

The only way to call a game function. A `Call` holds the parameter block of one `UFunction`; `Fire` runs it behind a crash marker.

**Thread:** game. Building a `Call`, `Lookup` and `Fire` throw `Refused` on any other thread, so no native code runs there.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Call(table, verb, on)` | binds row `verb` of the verb table `table` (the add-on's `App::specs`), on the object `on` |
| `Call(owner, name, on)` | binds a function by owner class and name, without a table row |
| `Set<T>(name, value)` | writes a parameter; the size must equal `sizeof(T)` |
| `Put(name, source, bytes)` | writes a parameter from raw bytes |
| `Try<T>(name, value)` | writes a parameter only when it exists with that size; silent otherwise, and without the checks below |
| `Wide(name, value)` | writes an integer parameter of 1 to 8 bytes |
| `CopyFrom(name, source, sourceName)` | copies a parameter from another `Call` |
| `Get<T>(name)` | reads a parameter or the result after `Fire`; the size must equal `sizeof(T)` |
| `Yes(name)` | reads a `bool` result |
| `Widened(name)` | reads an integer parameter of 1 to 8 bytes as `int64_t` |
| `CopyText(name)` | reads an `FString` parameter or result as `std::wstring`; the game's own buffer is not freed, so the string the game made for the result stays allocated (a small leak per call, by design) |
| `Has(name)`, `Find(name)`, `Parameter(name)` | look up a parameter: `Has` says whether it exists, `Find` gives a `const ue::Param *` or `nullptr`, `Parameter` gives a `const ue::Param &` or throws |
| `Data()` | the 2048-byte parameter block, 16-byte aligned |
| `Key()` | the ledger key `Owner.Name` |
| `Function()` | the cached `Fn` |
| `Target()` | the object the call runs on |
| `Risky()` | whether the table row has `verbs::Mark::Risky` |
| `Fire(call)` | runs the call under its own key `Owner.Name`; marked when `Policy::marks` is `Marks::All` (the default) or the row is `Risky` |
| `Fire(call, key)` | runs the call under the ledger key `key` (for example `Template.Locate`); always marked |
| `Lookup(owner, name, target)` | the cached `Fn` of a function; the constructors use it |

| `Fn` field | What it holds |
|---|---|
| `function` | the `UFunction` |
| `owner` | its owner class |
| `sign` | its parameters, a `ue::Sign` |
| `missing` | true when the function was not found or its block does not fit 2048 bytes |
| `widened` | true when the lookup also searched the target's class (Legacy `Policy::widen`) |

What `Fire` does, in order:

1. It refuses (`Refused`) when the SDK stops or the game asked to exit ("engine calls are stopped"), off the game-thread pump, when the session is halted, and when the key is disabled after a crash ("disabled after a crash: <key>").
2. The first call of a marked key on this game build writes a crash marker first (`pending` in the ini, flushed). Under `Policy::strictMarks` a marker that cannot be written refuses the call ("crash marker unavailable: <key>").
3. It runs the function. When it returns, the key is proven for this build and later calls run without a marker write.
4. A native fault inside the call disables the key for later sessions (until "Спробувати ще / Try again" in Diagnostics) and throws `Fault`. Under `Halt::Session` it also stops every call for the session.

`Refused` and `Fault` both derive from `std::runtime_error` (`using std::runtime_error::runtime_error`). `Refused` means nothing ran: try again later. `Fault` means the game faulted inside the function and the key is now disabled. A plain `std::runtime_error` means a missing function, a missing parameter or a size that does not fit.

Under `Policy::verifiedOnly` (the default) the SDK refuses more before any native code runs: `Call(table, verb)` with a verb that `verbs::Prime` did not verify or a table other than `App::specs`; an explicit target that is `ue::Doomed`; a doomed object passed with `Set`, `Put` or `CopyFrom` to an object parameter; and a call without a target whose host object is missing ("host not ready: <key>"). `Set`, `Put` and `CopyFrom` also refuse to lend text (`ue::Lend`) into an `FString` parameter that the game writes back (an out parameter or the result).

The template's locate call (`src/core.cpp`):

<!-- doccheck: present body -->
```cpp
const zsdk::world::Grip &w = zsdk::world::Hold();
if (!w.stable || !zsdk::ue::Here(w.body, w.bodyType) || zsdk::ledger::Blocked(kLocateKey)) return;
try
{
    zsdk::Call c(kSpecs, kLocation, w.body);
    zsdk::Fire(c, kLocateKey);
    const zsdk::math::Vec3 at = c.Get<zsdk::math::Vec3>("ReturnValue");
    zsdk::Log("template: located at %.0f %.0f %.0f in %s", at.x, at.y, at.z, w.worldName.c_str());
}
catch (const zsdk::Fault &error) { zsdk::Log("template: %s", error.what()); }
catch (const zsdk::Refused &error) { zsdk::Log("template: refused, %s", error.what()); }
catch (const std::exception &error) { zsdk::Log("template: locate failed: %s", error.what()); }
```

## `zsdk/census.h`

The census: a sweep of the game's object array on the SDK worker that finds the game instance, the static call hosts and the host objects named in `App::census`.

**Thread:** `Current`, `Want`, `Epoch`, `Sweeps` and `Fresh`: any. `Host` and `Consoles`: game (they compare with the live world grip). The sweep and the hooks `census` and `complete` run on the worker.

**Stability:** Provisional

| Call | What it does |
|---|---|
| `Current()` | the last snapshot, a `std::shared_ptr<const Snapshot>`, or `nullptr` before the first sweep |
| `Epoch()` | the world epoch the census works for |
| `Sweeps()` | the number of finished sweeps |
| `Want(reason)` | asks for a sweep; it decides the timing only under the Legacy `CensusMode::OnDemand`, and only logs under the default `CensusMode::Interval` |
| `Fresh(snapshot, epoch, since)` | true when the snapshot belongs to world epoch `epoch` and was taken at or after `since` |
| `Host(slot)` | the live host object of `CensusSpec::hosts[slot]` from a fresh snapshot of this world, not doomed, or `nullptr` |
| `Consoles()` | true when every listed host is there; until then the world grip waits with `wait.consoles` |
| `kSlots` | 16, the most statics and hosts |
| `kInstanceCap` | 8, the most game instances one snapshot keeps |

| `Snapshot` field | What it holds |
|---|---|
| `epoch` | the world epoch of the sweep |
| `at` | `Now()` when the sweep finished |
| `gi` | the last game instance found |
| `gis` | up to `kInstanceCap` game instances |
| `statics` | the class default object of each `CensusSpec::statics` name |
| `hosts` | a live object of each `CensusSpec::hosts` name |
| `sweepMs` | how long the sweep took |
| `objects` | the number of objects in the array |

The census sweeps every 2 s until `hooks.complete` says the snapshot has everything, then every 10 s, and at once after a world change. `hooks.census` sees each new snapshot of a new world, and every snapshot while it is not complete. The template's `complete` hook and a game-thread read:

<!-- doccheck: present -->
```cpp
bool Complete(const zsdk::census::Snapshot &snapshot)
{
    return snapshot.gi && snapshot.statics[0] && snapshot.statics[1];
}

void Census()
{
    const std::shared_ptr<const zsdk::census::Snapshot> snap = zsdk::census::Current();
    if (snap && zsdk::census::Fresh(*snap, zsdk::world::Epoch(), 0)) zsdk::Note(1, "census: %d objects in %.1f ms, sweep %d", snap->objects, snap->sweepMs, zsdk::census::Sweeps());
}
```

## `zsdk/exe.h`

Facts about the game exe the add-on runs in.

**Thread:** any. `Hash()` reads the whole exe file: the SDK runs it once on the worker right after it binds the engine; add-ons read `Sha()`, `Short()` and `Hashed()` instead.

**Stability:** Stable

| Call | What it does |
|---|---|
| `File()` | the exe file name, for example `Stalker2-Win64-Shipping.exe` |
| `Name()` | the same name as printable ASCII |
| `Game(file)` | true for any `Stalker2-<name>-Shipping.exe`; `DllMain` does not load the add-on into other processes |
| `Steam(file)`, `Steam()` | true for the Steam exe name `kSteamFile`; on every other store build the add-on stays off and says "this game build is not supported" |
| `Base()` | the exe image base in memory |
| `ImageSize()`, `Stamp()` | the image size and the time stamp from the PE header, known at once |
| `Hash()` | computes the SHA-256 of the exe file |
| `Sha()` | the computed SHA-256 in upper-case hex, empty before the hash |
| `Short()` | the first 8 hex digits in lower case (the build id, for example `61bc1e03`), or `?` before the hash |
| `Hashed()` | true once the hash is known |
| `Build()` | the `guard::Build` this exe is, or `nullptr` |
| `Known()` | true when `Build()` is not `nullptr` |

From the FieldWatch sample's verdict:

<!-- doccheck: present body -->
```cpp
if (zsdk::exe::Hashed())
    zsdk::Log("fieldwatch: game build %s is %s", zsdk::exe::Short().c_str(), zsdk::exe::Known() ? "known to this ZoneSDK" : "not known to this ZoneSDK");
else if (!zsdk::exe::Steam())
    zsdk::Log("fieldwatch: %s is not the Steam build, game calls stay off", zsdk::exe::Name().c_str());
```

## `zsdk/glyphs.h`

The character ranges the SDK's paint font can draw.

**Thread:** any.

**Stability:** Stable

| Name | What it is |
|---|---|
| `Run` | a range of code points: `first` and `count` |
| `kRuns` | the drawable ranges: Basic Latin, Latin-1, the apostrophe U+02BC, Cyrillic with the Ukrainian letters (Ґ ґ included), general punctuation and № |
| `Drawable(point)` | true when the code point is in `kRuns`; `constexpr` |
| `Undrawn(utf8)` | the signs of a UTF-8 text that would not draw, as `" U+XXXX"` pieces, or an empty string |

The generic `texts` suite checks every language row with `Undrawn`.

<!-- doccheck: present body -->
```cpp
static_assert(zsdk::glyphs::Drawable(0x0490) && !zsdk::glyphs::Drawable(0x2603));
const std::string bad = zsdk::glyphs::Undrawn("Ґанок №5 — «так»");
if (!bad.empty()) zsdk::Log("these signs are not drawn:%s", bad.c_str());
```

## `zsdk/guard.h`

Byte guards and typed fields: how an add-on uses a raw offset or address of the game exe only when the exe still has the expected bytes. A failed guard switches off only the features that use it.

**Thread:** `Check`, `Of`, `Ready`, `Usable`, `Use`, `Hex`, `Matches`, `Base`, `At`, `Target`, `Resolve` and `Find`: any (they read image bytes through `ue::Copy`, which never faults). `Confirm` walks the game's reflection: game or worker. `Recheck`: the SDK calls it on the game thread when the exe hash lands.

**Stability:** Stable

**Internal:** `guard::detail` (the compile-time helpers behind the field proofs).

| `Build` field | What it holds |
|---|---|
| `id` | the build id, the first 8 hex digits of the SHA-256 |
| `sha256` | the full SHA-256 of the exe |
| `imageSize`, `stamp` | the PE image size and time stamp |
| `objectsRoot`, `objectsCount` | where the engine's object array lives in this build |

`k2bd3f115` (game version 2.1, Steam build 26123434) and `k61bc1e03` (game version 2.0.6) are the Steam builds ZoneSDK 1.0.0 knows. `kBuilds` lists every known build, `{ &k61bc1e03, &k2bd3f115 }` in 1.0.0; a patch release may add rows, so do not rely on its length or order. `exe::Build()` and `Find` look only there, and `kImageBase` is the exe's preferred base `0x140000000` (an `Anchor` address is relative to it).

| Type | What it is |
|---|---|
| `Guard` | one checked spot: `rva` and `bytes`, hex pairs separated by spaces, `??` for a byte that may differ |
| `Need` | what a set needs besides its bytes: `Bytes` (nothing), `ImageSize` (the image size of its build), `KnownBuild` (the image size and the exe SHA-256 of its build; the set waits until the hash is known) |
| `Set` | a named group of guards: `name` (unique, never `zsdk.*`), `build`, `guards`, `count`, `need` |
| `State` | a set's verdict: `Pending`, `Ok`, `Off` |
| `Report` | the result of `Check`: `ok` and `total` guards matched, `image` and `imageOk` (the image size and whether it fits the build), `known` (the SHA-256 equals the build's), `bad` (the RVAs that did not match), `state` |
| `Anchor` | a pattern resolved at run time: `va` (address at `kImageBase`), `bytes`, `disp` (where the 32-bit displacement sits in the pattern), `next` (where the next instruction starts) |
| `Proof` | how a `Field` is proven: `Guard`, `Rip`, `Reflected`, `Unproven` |
| `Field` | a typed offset or address with its proof |

| `Field` member | What it holds |
|---|---|
| `name` | the field name, for log lines |
| `at` | the offset (or the RVA for a `Rip` field) |
| `set`, `guard`, `offset` | the set, the guard index in it and the byte position of the value in the guard's bytes |
| `reflectedClass`, `reflectedProperty` | the class and property a reflected field is checked against |
| `proof` | the `Proof` |
| `Field(name, at, set, guard, offset)` | a guard field; `consteval`: it does not compile unless the guard's bytes hold `at` at `offset` (4 bytes little-endian, or 1 byte when `at` is below 0x80) |
| `Field(name, at, klass, property)` | a reflected field; at run time `Confirm` compares `at` with the property offset in the game's reflection |
| `Field::Rip(name, rva, set, guard, disp, next, addend)` | an address field; it does not compile unless the RIP-relative displacement in the guard reaches `rva` (plus `addend`, 0 by default) |
| `Field::Unproven(name, at)` | a field without a proof: it compiles but is never usable; the generic `offsets` suite wants its name with a reason in `tests/unproven-fields.txt` |
| `Raw()` | the bare number; an explicit escape for an add-on that gates by hand, and the `offsets` suite counts every use |

A `Field` has no conversion to an integer: an add-on gets the offset only through `Use`, `mem::Read` or `mem::Write`.

| Call | What it does |
|---|---|
| `Check(set)` | checks the set now and stores the verdict |
| `Of(set)` | the stored verdict; a set that is still `Pending` is checked at once when the exe hash is known |
| `Ready(set)` | true once the set has a verdict (not `Pending`) |
| `Recheck()` | checks every `Pending` set again; the SDK runs it after the exe hash |
| `Usable(field)` | true when the field may be used: its set is `Ok`, or its reflection check passed |
| `Confirm(field)` | checks a reflected field against the game's reflection and keeps the result |
| `Use(field, at)` | gives the offset in `at` and true when the field is usable; otherwise false, with a log line (at most once a minute per field) that says why |
| `Hex(bytes, out)` | parses a guard text into byte values, -1 for `??` |
| `Matches(at, bytes)` | compares memory at an address with a guard text |
| `Base()` | the exe base, as `exe::Base()` |
| `At(anchor)` | the anchor's address in this process |
| `Target(at, disp, next)` | the RIP-relative target of the instruction at `at` |
| `Resolve(anchor)` | the anchor's target when its bytes match, else 0 |
| `Find(sha)` | the known `Build` with this SHA-256, or `nullptr` |
| `Own()` | the SDK's own sets: one set per known build for the game-exit watcher, `zsdk.exit.61bc1e03` and `zsdk.exit.2bd3f115`; the watcher uses the one that matches the running exe, and the generic `guards` suite checks them with the add-on's |

The FieldWatch sample's `src/fields.h` (one guard set and one guard field per known game build, one reflected field):

<!-- doccheck: present group=camera -->
```cpp
namespace guard = zsdk::guard;
inline constexpr guard::Guard kCameraGuards61bc1e03[] = {
    { .rva = 0x23CA0E4, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 0F 57 C9 0F 2E C1 77 22 48 8B 01 48 8B 80 A0 07 00 00 48 8D 15 ?? ?? ?? ?? 48 39 D0 75 11 48 81 C1 C0 17 00 00 F3 0F 10 41 30" },
};
inline constexpr guard::Guard kCameraGuards2bd3f115[] = {
    { .rva = 0x22B5CDE, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 0F 57 C9 0F 2E C1 77 22 48 8B 01 48 8B 80 A0 07 00 00 48 8D 15 ?? ?? ?? ?? 48 39 D0 75 11 48 81 C1 C0 17 00 00 F3 0F 10 41 30" },
};
inline constexpr guard::Set kCamera61bc1e03{ "fieldwatch.camera.61bc1e03", &guard::k61bc1e03, kCameraGuards61bc1e03, 1, guard::Need::KnownBuild };
inline constexpr guard::Set kCamera2bd3f115{ "fieldwatch.camera.2bd3f115", &guard::k2bd3f115, kCameraGuards2bd3f115, 1, guard::Need::KnownBuild };
inline constexpr guard::Field kDefaultFov{ "Camera.DefaultFOV", 0x2C8, "PlayerCameraManager", "DefaultFOV" };
inline constexpr guard::Field kLockedFov61bc1e03{ "Camera.LockedFOV", 0x2CC, kCamera61bc1e03, 0, 8 };
inline constexpr guard::Field kLockedFov2bd3f115{ "Camera.LockedFOV", 0x2CC, kCamera2bd3f115, 0, 8 };
inline constexpr const guard::Set *kSets[] = { &kCamera61bc1e03, &kCamera2bd3f115 };
inline constexpr const guard::Field *kLockedFovs[] = { &kLockedFov61bc1e03, &kLockedFov2bd3f115 };
```

The sample's `App` then sets `a.sets = fields::kSets;`. Its `hooks.prime` calls `zsdk::guard::Confirm(fields::kDefaultFov)` once. It reads `LockedFOV` through the field of `kLockedFovs` whose set `zsdk::guard::Of` reports `Ok`, and its job shows that set's verdict; while no set is on, the first field stands in and is refused.

## `zsdk/hooks.h`

The table of callbacks an add-on gives the SDK. Every field is optional (`nullptr`); the template wires the ones it uses in `src/entry.cpp`.

**Thread:** each hook runs on the thread in the table below. `hooks::Bind` runs in `DllMain`; `hooks::Get` is any.

**Stability:** Stable

| `Hooks` field | Thread | When it runs and what it does |
|---|---|---|
| `boot()` | present | once, at the first present, after the data folder and the language are set and before the SDK worker starts; the only place for `job::Every`; `shell::Ready()` turns true when it returns |
| `refresh(again)` | game | at prime and before every step; re-read settings when `ini::Revision()` changed; `again` is true right after the exe hash landed and the guard sets were checked again |
| `prime()` | game | once, after the engine is wired, `verbs::Prime` ran and the listed guard sets were checked |
| `advance()` | game | at every engine frame, before the world watch |
| `step(now)` | game | once per engine frame; the add-on's main game work |
| `hot()` | game | after each step; return true while the add-on needs a step at every engine frame and a publish after each step; while it is false and no request waits, the SDK wakes the game thread about once a second |
| `publish(now)` | game | every 250 ms, or every step while hot; copy the state other threads show, under the add-on's own lock |
| `turn(turn)` | game | when the world changes (`world::Turn`); drop pointers into the old world |
| `order(order)` | game | for each request from `thread::Ask`, at most 2 per step; requests older than 2 s or from before a world change are dropped |
| `orderName(order)` | game | a name of a request for log lines |
| `quit()` | game or worker | once, when the game asks to exit; engine calls are already stopped |
| `census(snapshot)` | worker | after a sweep of a new world, and after every sweep while the snapshot is not complete |
| `complete(snapshot)` | worker | return true when the snapshot holds everything the add-on needs; without the hook, a snapshot with a game instance is complete |
| `frame(input)` | present | at every present; keys and quick checks only, with the `Input` of this frame |
| `overlay(o)` | overlay | at every frame, inside ReShade's overlay event, whether or not the ReShade overlay is open; draw a HUD with `zsdk::paint`; the SDK draws the toasts after it |
| `page(o)` | overlay | the add-on's tab in the ReShade overlay |
| `opened(open)` | present | when the ReShade overlay opens or closes; outside the ImGui frame, so no ImGui here |
| `stop()` | loader | when the add-on unloads or the process exits, after the SDK stopped its own threads; inside the fence, where a fault or an exception ends it without a log line |
| `words(item, ui)` | overlay | the text of an add-on toast in the language `ui`; without it the add-on's toasts are not drawn |
| `tone(item)` | overlay | the strip color of an add-on toast (an ImGui color); default `tab::kAccent` |
| `pipelineInit(device, layout, count, subobjects, pipeline)` | D3D | ReShade's `init_pipeline` event |
| `pipelineBind(list, stages, pipeline)` | D3D | ReShade's `bind_pipeline` event; a hot path: keep it O(1) and lock-free |
| `pipelineDestroy(device, pipeline)` | D3D | ReShade's `destroy_pipeline` event |
| `deviceDestroy(device)` | D3D | ReShade's `destroy_device` event |
| `report()` | overlay | extra lines for "Скопіювати звіт / Copy report" in Diagnostics |

The GPU hooks are registered only when the add-on sets them. Handles arrive as `uint64_t` and the stage mask as `uint32_t`. `hooks.h` declares `reshade::api::device`, `reshade::api::command_list` and `reshade::api::pipeline_subobject` only; include ReShade's `reshade_api*.hpp` headers (never `reshade.hpp`) to use them. The D3D hooks run in parallel on several threads: no `Fire`, no `world::Hold`, no ImGui, no crash windows and no ini writes there.

`quit` runs once per process, on the game thread or on the SDK worker, never at the same time as `step`. It starts when the game sets its own exit flag (read only where the guard set of a known build, `2bd3f115` or `61bc1e03`, matches the running exe) or when the hook sees `WM_QUIT`. There is no `Fire`, `Call` or `mem::Write` in it; it may write a file, take its own locks and tell its own work to stop, briefly. When no such set matches, for example on a game build the SDK does not know, only `WM_QUIT` is left, and this game rarely sends it, so nothing that must survive every exit belongs in `quit`.

`stop` runs under the Windows loader lock. At a process exit (`shell::Exiting()` true) every other thread is already gone: take no lock, wait for nothing, write no file and call no engine function.

Every hook is guarded. A C++ exception from a hook is caught and logged; `stop` drops both kinds of exception without a log line, because it runs under the loader lock. A structured exception (an access violation, for example) in a game-thread hook is logged as "structured exception 0x<code> at <module>+0x<offset> in the game thread tick", and under `Policy::haltOnSeh` (the default) it stops every engine call for the session (`Phase::Halted`). A structured exception in `census` or `complete` switches both off for the session and disables the ledger key `census.hook` until "Спробувати ще / Try again".

Every ReShade callback of the add-on runs inside the SDK fence, in a lane of its own. A structured exception there switches the lane off for the session, logs one line, "structured exception 0x<code> at <module>+0x<offset> in the <lane> lane, <scope> for this session and present.<lane> is disabled until Try again", where <scope> is "this lane is off", "the frame and toasts lanes are off" or "the add-on is off", and writes the log to disk at once; a call through a null pointer is named "a call through a null pointer from <module>+0x<offset>". The ledger key `present.<lane>` stays disabled in later sessions until "Спробувати ще / Try again". A C++ exception in a callback is still caught by the inner handler and logged as "<lane> error: ..." at most once a minute per lane, and the lane stays on. Every unit of an add-on builds with `/EHa`, so the locks that the stopped code held are released. A thread with a stack of 1 MB or more gets a 32 KB stack guarantee the first time it enters the fence, so a stack overflow in a callback is fenced too.

| Lane | Ledger key | What runs in it |
|---|---|---|
| boot | `present.boot` | the add-on's start at the first present: the ini, the language, `boot` and the SDK threads |
| core | `present.core` | the SDK's own work at every present: the game-thread hook, the pump watch and the pump wake |
| frame | `present.frame` | `frame` with the `Input` of the frame |
| data | `present.data` | the settings tick: the delayed save and the log flush |
| block | `present.block` | the SDK's font texture upkeep before the overlay |
| overlay | `present.overlay` | `overlay` |
| toasts | `present.toasts` | the toasts |
| page | `present.page` | `page` |
| open | `present.open` | `opened` |
| device | `present.device` | the SDK frees its font textures of that device at `destroy_device` |
| device destroy | `present.deviceDestroy` | `deviceDestroy` |
| runtime | `present.runtime` | the toast reset at `destroy_effect_runtime` |
| save | `present.save` | the settings save and the log flush at `destroy_effect_runtime` |
| pipeline init, bind, destroy | `present.pipelineInit`, `present.pipelineBind`, `present.pipelineDestroy` | `pipelineInit`, `pipelineBind`, `pipelineDestroy` |

A fault in the frame or the toasts lane switches both off. The data lane runs only SDK code and goes off alone. A fault in boot or core switches the whole add-on off except the device, runtime and save lanes, so the SDK still cleans up and saves at the end. While the page lane is off, the add-on's tab shows the SDK's own text `sdk.lanes` with the add-on name and the parts that are off (their `lane.*` rows), the log path, the Try again button and "Скопіювати звіт / Copy report", and `sdk.restart` after a fault in boot or core. Try again lets the lanes run again at once; after a fault in boot or core in the same session, the add-on itself starts again with the next game start. A later start whose keys are still disabled logs `safety: the <lanes> lanes are disabled after a crash in an earlier session, Try again in the tab clears them`.

The GPU hooks and `deviceDestroy` can come before the first present. Then the SDK first opens the data folder, in the boot lane, so a key disabled in an earlier session holds there too. A lane that faults before the data folder is open stays disabled once it is, with the line `safety: the <lanes> lanes faulted before the data folder opened, they stay disabled`.

`hooks::Bind(hooks)` stores the table (`shell::Main` calls it) and `hooks::Get()` returns it. Hooks are named functions: the generic `present` suite refuses lambdas and checks that `page` and `overlay` point to overlay functions and the others to present functions. The template's `src/entry.cpp`:

<!-- doccheck: entry -->
```cpp
zsdk::Hooks Wire()
{
    zsdk::Hooks h;
    h.refresh = &ztemplate::core::Refresh;
    h.step = &ztemplate::core::Step;
    h.order = &ztemplate::core::Order;
    h.frame = &ztemplate::frame::Frame;
    h.page = &ztemplate::page::Page;
    h.words = &ztemplate::page::Words;
    return h;
}
```

## `zsdk/ini.h`

Settings and the add-on's data files. One visitor function describes the settings struct; the SDK reads, clamps, writes and diffs it, keeps unknown keys and keeps the crash ledger in the same ini file.

**Thread:** any (one lock). `Open`, `Close` and `Tick` are the SDK's: `Open` at the boot, `Tick` at every present (it flushes the log every 500 ms and saves changed settings 1 s after the last change), `Close` at an unload. File calls (`Save`, `Store`, `WriteAtomic`, `ReadFile`) do not belong in the D3D hooks or in `stop`.

**Stability:** Stable

**Internal:** `ParseRaw`, `TextRaw`, `SanitizeRaw`, `ReadRaw`, `ApplyRaw` (the untyped work behind the typed calls below).

The settings struct is plain data with `operator==`. The add-on describes it with one function that calls the `Visitor`:

| `Visitor` call | Setting |
|---|---|
| `Flag(section, key, value)` | a `bool`, written `1` or `0` (`true`, `yes` and `on` also read) |
| `Whole(section, key, value, def, low, high)` | an `int` clamped to `low`..`high` |
| `Real(section, key, value, def, low, high)` | a `double` clamped to `low`..`high` (`def` when it is not a number), written with `Real` |
| `Word(section, key, value, def, choices, count)` | a string; with `choices`, one of `count` lower-case words, else `def`; without, free text |
| `Pick(section, key, value, names, count, def)` | an enum stored by name |
| `Key(section, key, value, def)` | a `keys::Chord`, written by name such as `ALT+L`; `none` for no key, `def` for a key that cannot be bound |
| `Text(section, key, value, most)` | free text up to `most` bytes (at most `Limits::kTextMost`), control characters removed |

The defaults are the settings struct's own member initializers: a missing or unreadable key keeps them. The SDK implements `Visitor` (it has a virtual destructor, `~Visitor`); the add-on's visit function only calls it. `Choice(v, section, key, value, names, def)` is `Pick` for an `enum class` with a names array. Section and key names are lower case. The sections `safety` and `proven` and the add-on's owned sections are reserved.

| `Schema` field | What it holds |
|---|---|
| `head` | the section of keys before the first `[section]` line, `"general"` |
| `visit` | the visitor function |
| `fix` | runs after a parse and after `Apply`, to repair values that depend on each other |
| `make`, `drop`, `copy`, `same` | type-erased new, delete, copy and compare of the settings struct |
| `level` | gives the log level (0 to 2) from the settings |
| `language` | gives the language code (`auto`, `uk`, `en`) from the settings |
| `owned`, `ownedCount` | the `Owned` sections the add-on keeps as plain key/value maps |

`Make<S, Visit, Fix, Level, Language>(head, owned, ownedCount)` builds a `constexpr` `Schema` from typed functions. It uses `Typed<S>` (with `Make`, `Drop`, `Copy`, `Same`) and the adapters `VisitThunk`, `FixThunk`, `LevelThunk` and `LanguageThunk`. An `Owned` section has a `name` and a `cap` (the most keys it keeps).

| Call | What it does |
|---|---|
| `Config<S>()` | a copy of the live settings |
| `Apply<S>(settings)` | clamps the settings (the visitor, then `fix`) and stores them when they changed; saved about 1 s later |
| `Revision()` | a number that grows when the settings change (`Apply`), an owned section is replaced (`Store`; `Put` does not raise it), the disabled list changes, or `Touch` is called; compare it to re-read settings cheaply |
| `Touch()` | raises `Revision()` without a change |
| `Dirty()` | true while changed settings wait to be saved |
| `Save()` | writes the ini now |
| `Parse<S>(text, settings, keep)` | reads ini text into a settings struct, and the other sections into a `Kept` when `keep` is given |
| `Text<S>(settings, keep)` | the ini text (CRLF lines) of settings and a `Kept` |
| `Sanitize<S>(settings)` | clamps a copy |
| `Check(make)` | throws `std::logic_error` when the struct type differs from the bound schema; the typed calls use it |
| `Bound()` | the bound schema (`App::schema`) |
| `Language()` | the language code from the settings, or `auto` |
| `Ledger()` | a copy of the ledger block, a `Kept` |
| `Folder()`, `LogPath()`, `ExitPath()` | the data folder, the log file and the exit mark file (`<ini name>.exit`) |
| `Open(folder)` | opens the data folder (default `%LOCALAPPDATA%\<Identity::folder>`), reads the ini and judges the last run; tests pass their own folder |
| `Close()` | saves and flushes |
| `Tick()` | the SDK's file tick |
| `Section(name)` | an owned section as a `std::map` |
| `Store(name, values)` | replaces an owned section and writes the ini now |
| `Put(name, key, value)` | sets one key of an owned section (at most its `cap` keys, 16 when it is not listed); saved later |
| `Stamp()` | the local time as `YYYY-MM-DD HH:MM` |
| `Real(value)` | a `double` as text with up to 4 decimals, no trailing zeros and no `-0` |
| `WriteAtomic(path, body)` | writes a file through a temporary file, flushed and renamed over the old one |
| `ReadFile(path, out, limit)` | reads a whole file of at most `limit` bytes (1 MiB by default) |

The ledger block of the ini is a `Kept`: `session` (this run's id), `pending` (the call whose crash marker is set), `disabled`, `watch` (open crash windows), `proven` (key to build id), `owned` (the owned sections), `unknown` (unknown keys, each an `Entry` with `section`, `key` and `value`) and `addon` (the add-on build that wrote the proofs). `Kept` and `Entry` compare with `==`. `Limits` holds the caps: `kUnknownKept` 64 unknown keys, `kDisabledKept` 32 disabled keys, `kWatchKept` 8 and `kWatchCap` 32 crash windows, `kTextMost` 256 bytes of free text.

The template's `src/settings.cpp`:

<!-- doccheck: present group=settings -->
```cpp
constexpr const char *kLanguages[] = { "auto", "uk", "en" };

void Visit(Settings &s, zsdk::ini::Visitor &v)
{
    v.Word("general", "language", s.language, "auto", kLanguages, 3);
    v.Flag("general", "enabled", s.enabled);
    v.Flag("general", "toasts", s.toasts);
    v.Whole("general", "log_level", s.logLevel, 1, 0, 2);
    v.Key("keys", "locate", s.locate, Settings{}.locate);
}
```

<!-- doccheck: present group=settings -->
```cpp
void Fix(Settings &) {}
int Level(const Settings &s) { return s.logLevel; }
const char *Language(const Settings &s) { return s.language.c_str(); }
constexpr zsdk::ini::Owned kOwned[] = { { "memory", 16 } };
constexpr zsdk::ini::Schema kSchema = zsdk::ini::Make<Settings, Visit, Fix, Level, Language>("general", kOwned, 1);
```

A fresh template ini then reads:

```ini
[general]
language=auto
enabled=1
toasts=1
log_level=1

[keys]
locate=ALT+L

[safety]
session=<32 hex digits>
pending=
disabled=
watch=
addon=0.1.0/<8 hex digits>

[proven]

[memory]
```

## `zsdk/input.h`

The keyboard and mouse as ReShade sees them in this frame, and whether a key is meant for the add-on.

**Thread:** present. The `Input` passed to `hooks.frame` is valid only during that call.

**Stability:** Stable

| `Input` call | What it does |
|---|---|
| `Down(vk)`, `Pressed(vk)`, `Released(vk)` | a virtual key is held, went down this frame, went up this frame |
| `MouseDown(button)`, `MousePressed(button)` | a mouse button is held, went down this frame |
| `Cursor(x, y, wheel)` | the cursor position and, when `wheel` is given, the wheel movement |
| `Async(vk)` | the key state from Windows (`GetAsyncKeyState`) |
| `Mods()` | the held modifiers as `keys::kCtrl`, `kShift`, `kAlt` bits |
| `Fires(chord)` | the chord went down this frame with the right modifiers (`keys::Fires`) |
| `Focused()` | the game window has the focus |
| `Overlaid()` | the ReShade overlay is open |
| `Menu()` | the player is in a menu: the game is paused, or the OS cursor is shown and steady (400 ms without Alt, Ctrl or Shift and with the overlay closed) |
| `Paused()` | the game is paused |

Use keys only when `Focused()` and not `Overlaid()` or `Menu()`, and hand the work to the game thread with `thread::Ask`. The template's `src/frame.cpp`:

<!-- doccheck: present -->
```cpp
void Keys(const zsdk::Input &input)
{
    static uint64_t revision = 0;
    static Settings s;
    if (zsdk::ini::Revision() != revision)
    {
        revision = zsdk::ini::Revision();
        s = zsdk::ini::Config<Settings>();
    }
    if (!s.enabled || !input.Focused() || input.Overlaid() || input.Menu()) return;
    if (input.Fires(s.locate)) zsdk::thread::Ask(static_cast<int>(Order::Locate));
}
```

## `zsdk/job.h`

Background work without own threads: periodic jobs that run on the add-on's SDK worker.

**Thread:** `Every`: only from `hooks.boot`. The job functions run on the worker. `Stopping` and `Off`: any.

**Stability:** Provisional

| Call | What it does |
|---|---|
| `Every(name, periodMs, run)` | registers `run(now)` to run every `periodMs` or more; false (with a log line) when it is refused; the period is a minimum, coarsened by the worker's 100 ms tick |
| `Stopping()` | true while the SDK stops, after the game asked to exit and at the process exit; a long job polls it and returns |
| `Off(name)` | true when the job is switched off or disabled |
| `kMost` | 8, the most jobs of one add-on |
| `kShortestMs` | 100, the shortest period |
| `kBudgetMs` | 50, the time one pass may spend starting jobs; the next jobs wait for the next pass |
| `kSlowMs` | 50, a run longer than this is logged |
| `kStallMs` | 1000, a run longer than this switches the job off for the session |

A job name is 1 to 32 letters, digits, `.`, `_` or `-`. Jobs run one after another on every game build, also on builds where the engine is not bound. A job makes no game calls (`Call` and `Fire` refuse off the game thread): it hands its result to the game thread under its own lock and with `thread::Ask`. A structured exception in a job switches it off for the session and disables the ledger key `job.<name>` until "Спробувати ще / Try again"; a C++ exception is logged at most once a minute and the job keeps its period. Nothing stops a job that never returns: after `kStallMs` the SDK logs once "job <name> has not returned for <n> ms, the SDK worker waits for it", and no other job, census step or exit poll runs until it returns. The generic `hardrules` suite refuses own threads, timers and thread-pool callbacks in add-on sources and points to `job::Every`. The FieldWatch sample's verdict job:

<!-- doccheck: present -->
```cpp
void Judge(uint64_t)
{
    if (zsdk::job::Stopping() || zsdk::job::Off("fieldwatch.verdict")) return;
    zsdk::Note(2, "fieldwatch: the game build is %s", zsdk::exe::Known() ? "known" : "not known yet");
}

void Boot()
{
    if (!zsdk::job::Every("fieldwatch.verdict", 500, &Judge)) zsdk::Log("fieldwatch: the verdict job was refused");
}
```

## `zsdk/keys.h`

Key chords: parse, name, show and match keys the way every ZoneSDK add-on does.

**Thread:** any.

**Stability:** Stable

A `Chord` is a virtual key `key` and modifier bits `mods` (`kCtrl`, `kShift`, `kAlt`); chords compare with `==`.

| Call | What it does |
|---|---|
| `Parse(text)` | `"ALT+K"`, `"CTRL+SHIFT+F7"`, `"PAGEUP"` and so on to a chord; an unknown text or `NONE` gives an empty chord |
| `Name(chord)` | the stored name, for example `ALT+K`, or `none` |
| `Shown(chord, lang)` | the name a player reads, for example `Alt+K` |
| `Glyph(key, lang)` | the shown name of one key; in Ukrainian, symbol keys also name the Ukrainian letter on them |
| `Known(key)` | the key has a name |
| `Modifier(key)` | Shift, Ctrl, Alt or Windows |
| `Bindable(chord)` | a known key that is not a modifier, Esc or Tab |
| `Game(key)` | the game uses this key on its own (movement, weapons, Space, Tab, =, F5, F8 and others) |
| `Numpad(key)` | a numpad key; ZoneSDK never uses one as a default, many keyboards have none |
| `Mods(ctrl, shift, alt)` | modifier bits from three flags |
| `Match(chord, key, mods)` | the Alt rule: a chord with modifiers needs exactly them; a plain key fires with Ctrl or Shift held, but not with Alt |
| `Exact(chord, key, mods)` | the modifiers must be equal |
| `Fires(chord, key, mods)` | `Match` or `Exact`, as `Policy::mods` says |
| `Which(next, back, key, mods)` | 1 for `next`, -1 for `back`, 0 for neither, for a pair of keys that may share a key |

Names: `A`-`Z`, `0`-`9`, `F1`-`F24`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `TAB`, `ENTER`, `SPACE`, `BACKSPACE`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `CAPSLOCK`, `SCROLLLOCK`, `PAUSE`, `OEM102`, the symbols `. , - = [ ] ; ' / \` and the backquote, and the numpad keys `NUM0`-`NUM9`, `NUMMULTIPLY`, `NUMADD`, `NUMSUBTRACT`, `NUMDECIMAL` and `NUMDIVIDE`.

<!-- doccheck: present body -->
```cpp
const zsdk::keys::Chord chord = zsdk::keys::Parse("ALT+K");
const bool fine = zsdk::keys::Bindable(chord) && !zsdk::keys::Numpad(chord.key) && !zsdk::keys::Game(chord.key);
zsdk::Log("%s, shown as %s, %s", zsdk::keys::Name(chord).c_str(), zsdk::keys::Shown(chord, zsdk::lang::Lang::Uk).c_str(), fine ? "a good default" : "pick another key");
const bool fires = zsdk::keys::Fires(chord, 'K', zsdk::keys::Mods(false, false, true));
zsdk::Note(2, "Alt+K fires: %s", fires ? "yes" : "no");
```

## `zsdk/lang.h`

The two languages of every ZoneSDK add-on, Ukrainian and English, and the text rows.

**Thread:** any. `Detect()` reads the game settings once and caches the result; the SDK runs it at the boot.

**Stability:** Stable

A `Row` has an `id` and one `text` per language, Ukrainian first: `{ "say.located", { "Ви тут: {0}", "You are here: {0}" } }`. `Lang` has the values `Uk`, `En` and `Count`.

| Call | What it does |
|---|---|
| `Current()` | the language in use |
| `Set(lang)` | sets it |
| `Set(code)` | `uk` or `en`; `auto`, an empty, unknown or Russian code all select `Detect()` |
| `Detect()` | the game language: uk gives `Uk`, every other language gives `En`; decided once from the game's settings, then Steam, then Windows, and logged |
| `Detect(roots)` | the same decision for the given `paths::Roots`, without the cache, for tests |
| `Code(lang)` | `uk` or `en` |
| `Valid(code)` | true for `auto`, `uk` and `en` |
| `Text(id)`, `Text(id, lang)` | the text of a row in the current or the given language: the add-on's row first, then the SDK default row; `?` for an unknown id |
| `Has(id)` | a row with this id exists |
| `Ids()` | the ids of the add-on's own rows |
| `Defaults()` | the SDK's default rows (`phase.*`, `diag.*`, `key.*`, `wait.consoles`, `sdk.unsupported`, `sdk.lanes`, `sdk.restart`, `lane.*`, `yes`, `no`) |
| `Format(pattern, args)` | fills `{0}` to `{99}` with the arguments; two overloads, a list or a vector |
| `Slots(pattern)` | the slot numbers a pattern uses |
| `Russian(utf8)` | true when a text has a Russian-only letter or a Russian word; the generic `texts` suite and the release check use it |

There is no Russian option: every language other than Ukrainian gives English. The generic `texts` suite checks that both texts of a row exist, use the same slots and draw with the SDK font. The template's rows and a toast text:

<!-- doccheck: present -->
```cpp
constexpr zsdk::lang::Row kRows[] = {
    { "say.located", { "Ви тут: {0}", "You are here: {0}" } },
    { "why.world", { "світ гри ще не готовий", "the game world is not ready" } },
};

std::string Located(const std::string &where)
{
    return zsdk::lang::Format(zsdk::lang::Text("say.located"), { where });
}
```

## `zsdk/ledger.h`

The crash ledger: which game calls and memory windows ran, which crashed the game and stay off, and whether the last run ended cleanly. It lives in the `[safety]` and `[proven]` sections of the add-on's ini.

**Thread:** any (one lock; calls that change the ledger write the ini). A `Window` around game memory work belongs on the game thread.

**Stability:** Stable

**Internal:** `ledger::Calm` (the SDK's exit sequence clears the open windows with it).

How it works: before a guarded call or memory window runs for the first time on a game build, its key is written to the ini. When it returns, the key is proven for that build. When the game crashes while a key is written, the next run disables that key: the feature stays off until the player presses "Спробувати ще / Try again" in Diagnostics (`Retry`). A clean game exit leaves an exit mark next to the ini, so a normal quit is never taken for a crash.

| Call | What it does |
|---|---|
| `Blocked(key)` | true when the key is disabled after a crash |
| `Disabled()` | every disabled key |
| `Booted()` | the keys this boot disabled (the game ended while they were open) |
| `Retry()` | clears the disabled list; Diagnostics calls it |
| `Full()` | the disabled list holds 32 keys; a new one drops the oldest |
| `Proven(key)` | the build id the key is proven for, or empty |
| `Preloaded(key)`, `Preloaded(key, build)` | the key is proven in the SDK's list for this (or that) build, `data/builds/<id>/proven.txt` |
| `PreloadCount()` | the number of preloaded keys |
| `Watched()` | the keys of the crash windows open now |
| `Watch(key)`, `Unwatch(key, build)` | open and close a crash window by hand; `Window` does this for you |
| `WatchLimit()` | the most open windows, `Policy::watchKept` (1 to 32, otherwise 8) |
| `Addon()` | the add-on build, `<version>/<8 hex digits of the image time stamp>`; proofs of another build are dropped at the boot |
| `CleanStart()` | the last run ended with a clean exit mark |
| `Last()`, `Current()` | the session id of the last run and of this run |
| `Closed()` | the windows the last run left open that a clean exit closed |
| `Stuck()` | the old exit mark could not be removed |

A `Window` is a crash window around a stretch of game memory work:

| `Window` call | What it does |
|---|---|
| `Window(key)` | opens the window: writes the key as watched unless it is proven for this build; refused when the key is disabled or cannot be written |
| `Close()` | closes the window and proves the key for this build |
| `~Window()` | a window destroyed without `Close` stops watching without a proof |
| `Open()`, `Armed()`, `Refused()` | the window is open, its key was written, it was refused |
| `Key()` | its key |

A `Window` cannot be copied. Keys are short `Addon.Feature` names; a key the game crashed in is reported in the log, in Diagnostics and in the report. The template's prime hook:

<!-- doccheck: present body -->
```cpp
for (const std::string &key : zsdk::ledger::Booted()) zsdk::Log("template: %s was disabled at this boot, Try again in the tab clears it", key.c_str());
if (zsdk::ledger::Blocked(kLocateKey)) zsdk::Log("template: the locate call stays off");
if (!zsdk::ledger::CleanStart()) zsdk::Note(1, "template: the last run %s did not end cleanly", zsdk::ledger::Last().c_str());
const std::string proof = zsdk::ledger::Proven(kLocateKey);
zsdk::Note(2, "template: %s proven for build %s", kLocateKey, proof.empty() ? "-" : proof.c_str());
```

## `zsdk/log.h`

The add-on's log file in its data folder.

**Thread:** any. Lines wait in a queue (at most 4096; the oldest are dropped and counted) and the SDK writes them every 500 ms from the present tick. At the process exit the calls only try their locks and drop a line when a lock is busy.

**Stability:** Stable

**Internal:** `log::Hushed`, `log::Due` (the SDK's flush timing).

| Call | What it does |
|---|---|
| `Log(shape, ...)` | one `printf`-style line, always written; a line is at most 4095 bytes |
| `Note(level, shape, ...)` | a line written only when `level` is at most the log level (`log_level` in the settings, 0 to 2, default 1) |
| `Rare(tag, shape, ...)` | a line written at most once per 60 s for each `tag`; a later line says how often it repeated |
| `Flush()` | writes the queued lines now |
| `Urgent()` | makes the next file tick write at once |
| `log::Path()` | the log file |
| `log::Level()`, `log::SetLevel(level)` | the log level; the SDK sets it from the settings |
| `log::Pending()`, `log::Dropped()` | queued lines, and lines dropped because the queue was full |
| `log::Open(file, rolled)`, `log::Roll(file, rolled)` | the SDK's: open the log, move it to the rolled name (at 8 MiB while running, 4 MiB at the start) |
| `log::Masked(utf8)` | a text with `%LOCALAPPDATA%`, `%USERPROFILE%` and the Windows user name hidden |
| `log::Shown(path)` | a path the same way, for log lines and the tab |

A line looks like `12:34:56.789 [1234] text`, with the thread id in brackets.

<!-- doccheck: present body -->
```cpp
zsdk::Log("template: located at %s", "1 2 3");
zsdk::Note(2, "template: detail only at log level 2, now %d", zsdk::log::Level());
zsdk::Rare("template locate", "template: locate failed again");
zsdk::Log("template: the log is %s", zsdk::log::Shown(zsdk::log::Path()).c_str());
```

## `zsdk/math.h`

Vectors and rotations in the game's units: centimetres and degrees, X forward, Y right, Z up, yaw around Z.

**Thread:** any.

**Stability:** Stable

`Vec3` has `x`, `y`, `z`; `Rot3` has `pitch`, `yaw`, `roll`; both compare with `==` and match the game's `FVector` and `FRotator` doubles, so `Call::Get<zsdk::math::Vec3>` reads a location result.

| Call | What it does |
|---|---|
| `Add(a, b)`, `Sub(a, b)`, `Mul(a, k)` | vector sum, difference and scale |
| `Dot(a, b)`, `Cross(a, b)` | dot and cross product |
| `Length(v)`, `Unit(v)` | length; unit vector (`{0, 0, 1}` for a zero vector) |
| `Flat(a, b)` | distance on the ground, without Z |
| `Wrap(yaw)` | an angle in -180..180 |
| `Forward(heading)`, `Left(heading)`, `Right(heading)` | ground directions of a yaw |
| `Across(pos, heading, f, l)` | a point `f` ahead and `l` to the left on the ground |
| `Ahead(rot)` | the direction a rotation looks |
| `Axes(rot, x, y, z)`, `FromAxes(x, y, z)` | a rotation to its three axes and back |
| `Attitude(heading, pitch, roll)` | the rotation that turns a mesh whose forward is its +Y axis to face `heading`, with `pitch` and `roll` |
| `Compose(outer, inner)` | the rotation `inner` applied inside `outer` |
| `Place(origin, rot, flu)`, `Local(origin, rot, world)` | a forward, left, up offset in the frame of such a +Y-forward rotation to a world point, and back: `flu.x` runs along the rotation's Y axis, `flu.y` along its X axis, `flu.z` along Z |
| `Quarter(meshYaw, c, s)` | cosine and sine of a mesh yaw, exact for 0, 90, 180 and 270 |
| `MeshHalf(f, l, u, meshYaw)` | half extents of a box turned by a mesh yaw |

<!-- doccheck: present body -->
```cpp
const zsdk::math::Vec3 at{ 100.0, 200.0, 50.0 };
const zsdk::math::Rot3 rot{ 0.0, zsdk::math::Wrap(270.0), 0.0 };
const zsdk::math::Vec3 ahead = zsdk::math::Add(at, zsdk::math::Mul(zsdk::math::Ahead(rot), 300.0));
zsdk::Note(2, "3 m ahead is %.0f %.0f, %.0f cm away on the ground", ahead.x, ahead.y, zsdk::math::Flat(at, ahead));
```

## `zsdk/mem.h`

Raw game memory through typed fields. Reads go through the SDK's safe copy; writes need an open crash window on the game thread.

**Thread:** `Read`: any (it never faults), usually game. `Write`, `Put`, `Poke` and `Writable`: game, inside the pump.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Read(base, field, out)` | reads `out` at `base` plus the field's offset; false when the field is not usable (`guard::Use`) or the memory cannot be read |
| `Write(window, base, field, value)` | writes `value` at `base` plus the field's offset |
| `Write(window, at, value)` | writes at an address |
| `Put(window, at, from, bytes)` | writes raw bytes |
| `Writable(window)` | the window is open, not refused, its key is not disabled, and this is the game-thread pump |
| `Poke(at, from, bytes)` | writes without a window; only in files listed under `memory=` in `tests/zsdk.cfg` |
| `Pokes()` | the number of raw writes so far |

Every write refuses off the game-thread pump, without an open window and when the window's key is disabled. Raw writes keep nothing alive in the game: keep objects through the game's own references, and keep changes reversible within the session ([RULES.md](RULES.md#7-game-objects-and-game-state)). The FieldWatch sample picks the guarded field of the running build and reads it inside a crash window:

<!-- doccheck: present group=camera -->
```cpp
const guard::Field &Locked()
{
    for (const guard::Field *field : kLockedFovs)
        if (zsdk::guard::Of(*field->set) == zsdk::guard::State::Ok) return *field;
    return *kLockedFovs[0];
}

float LockedFov(const void *camera)
{
    zsdk::ledger::Window window("FieldWatch.LockedFov");
    float value = -1.0f;
    if (!window.Refused() && !zsdk::mem::Read(zsdk::Addr(camera), Locked(), value)) value = -1.0f;
    window.Close();
    return value;
}
```

## `zsdk/overlay.h`

The overlay side: the add-on's tab, the standard Diagnostics block and the widgets that draw with the SDK font. Include it only in `src/overlay` files; it `#error`s in a present unit.

**Thread:** overlay. Every widget checks that it runs inside the overlay or page event and otherwise returns at once with one log line. `tab::Fixed` and `tab::Fade` are plain math.

**Stability:** Stable

`Overlay` is passed to `hooks.overlay` and `hooks.page`: `runtime` (the ReShade effect runtime), `scale` (the screen height over 1080, at least 1) and `ui` (the language: `overlay::Ui()` in `page`, `lang::Current()` in `overlay`, so a HUD calls `overlay::Ui()` itself). `overlay::Inside()` is true inside those events; `overlay::Ui()` is the language to draw in: `lang::Current()`, or English when the font cannot draw Cyrillic. An `overlay::Scope` marks code as inside the overlay event; the SDK opens one around the hooks, and it cannot be copied.

| `tab` call | What it does |
|---|---|
| `Label(text, tone, framed)` | one line of text in the SDK font (Cyrillic-safe); `framed` aligns it with a widget |
| `Wrapped(text, tone)` | text wrapped at word boundaries |
| `Beside(text, tone)` | text on the line of the last widget |
| `Heading(text)` | a separator and an accent label |
| `Box(id, text, value, tone)` | a checkbox; true when it changed |
| `Whole(id, text, value, low, high)` | an integer slider |
| `Real(id, text, value, low, high, format, step)` | a slider for a `double`, rounded to `step` |
| `Button(id, text, width)` | a button; true when pressed |
| `Clipped(id, text, width)` | a button whose text is clipped to its width |
| `KeyField(o, id, chord, others, texts, width)` | a key capture button: 1 when a new chord was picked, -1 when Esc cancelled, 0 otherwise; refuses keys that are not `keys::Bindable` or are in `others`, and notes keys the game uses unless the chord holds ALT |
| `Capturing(id)` | that key field waits for a key |
| `Release()` | stops any key capture; the SDK calls it when the overlay closes |
| `Diagnostics(o, texts)` | the standard block: phase, game-thread counters, game build, functions found, calls disabled after a crash with "Спробувати ще / Try again", the log file, the ZoneSDK version and legacy rules, and "Скопіювати звіт / Copy report" |
| `Report(o, texts)` | builds the report, copies it to the clipboard, saves `report.txt` in the data folder and returns the status line |
| `Size()`, `Field()` | the font size, and a good width for a slider or key field (45 % of the row, 120 to 360 px) |
| `Fixed(value, digits)` | a number as text with fixed decimals |
| `Fade(tone, k)` | a color with its alpha scaled by `k` |

The colors are `kText`, `kDim`, `kAccent`, `kGood`, `kWarn` and `kShade` (the toast background). `KeyTexts` holds the texts of a key field: `label` (shown beside the button), `press`, `bad` and `game`. `DiagTexts` holds the texts of Diagnostics: `title`, `phase`, `exe`, `known`, `other`, `unknown`, `foreign`, `verbs`, `crashed`, `none`, `retry`, `log`, `sdk`, `yes`, `no`, `markers`, `full`, `pump`, `report`, `copied`, `unsaved` and `unsupported`. Every field defaults to `nullptr`, which takes the SDK default row in the overlay language, so `tab::Diagnostics(o)` needs no texts. `toast::Draw(o, lane, words, tone)` draws the toasts; the SDK calls it after `hooks.overlay`.

The template's `src/overlay/page.cpp`, shortened:

<!-- doccheck: overlay body -->
```cpp
Settings s = zsdk::ini::Config<Settings>();
const Settings before = s;
const zsdk::tab::KeyTexts texts{ .label = zsdk::lang::Text("key.locate", o.ui) };
zsdk::tab::Label(zsdk::lang::Format(zsdk::lang::Text("title", o.ui), { kVersion }), zsdk::tab::kAccent);
zsdk::tab::Box("enabled", zsdk::lang::Text("mod.enabled", o.ui), s.enabled);
zsdk::tab::KeyField(o, "locate", s.locate, {}, texts, std::min(zsdk::tab::Field(), 200.0f));
if (zsdk::tab::Button("locatebtn", zsdk::lang::Text("btn.locate", o.ui))) zsdk::thread::Ask(static_cast<int>(Order::Locate));
zsdk::tab::Diagnostics(o);
if (!(s == before)) zsdk::ini::Apply(s);
```

## `zsdk/paint.h`

Drawing primitives and the SDK's own font (with Cyrillic), for HUDs drawn in `hooks.overlay`. Include it only in `src/overlay` files (`zsdk/overlay.h` includes it).

**Thread:** overlay. Every call checks that it runs inside the overlay event. `Serve` and `Drop` are the SDK's; `ink`, `terminal`, `Theme` and `Probe` only read or set a pointer.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Box(sheet, a, b, tone)` | a filled rectangle |
| `Frame(sheet, a, b, tone, thick)` | a rectangle outline |
| `Rule(sheet, a, span, tone, thick)` | a horizontal line of length `span` |
| `Grain(sheet, a, b, step, tone)` | scan lines every `step` pixels |
| `Corners(sheet, a, b, tone, len, thick)`, `Bracket(...)` | corner ticks of a box |
| `Text(sheet, at, size, tone, line)` | a line of UTF-8 text |
| `Mid(sheet, at, size, tone, line)` | text centered on `at.x` |
| `Caps(sheet, at, size, tone, line, gap)` | letter-spaced text; returns its width |
| `Span(size, line)`, `CapsSpan(size, line, gap)` | the width a text would take |
| `Cyrillic()` | the font in use can draw Cyrillic |
| `ink()` | the palette in use; `terminal()` is a green terminal palette, and `Theme(palette)` makes `ink()` return another one (`nullptr` for the default) |
| `Serve(device)`, `Drop(device)` | the SDK's: keep the font texture of a device, and drop it with the device |
| `Probe(sink)` | calls `sink(pixels)` with the size of every text drawn, for tests and tools |

`sheet` is an ImGui draw list, usually `ImGui::GetForegroundDrawList()`. `paint.h` declares `reshade::api::device` only. An `Ink` palette has the roles `glass`, `edge`, `hair`, `amber`, `wash`, `paper`, `text`, `warm`, `dim`, `faint`, `tile`, `dark`, `foe`, `veiled`, `life`, `soot`, `glow` and `rail`. The HelloHud sample's corner panel, shortened:

<!-- doccheck: overlay -->
```cpp
void Panel(zsdk::Overlay &o, const std::string &line)
{
    namespace paint = zsdk::paint;
    if (!o.runtime || !zsdk::overlay::Inside()) return;
    ImDrawList *sheet = ImGui::GetForegroundDrawList(nullptr);
    const float size = 18.0f * o.scale;
    const ImVec2 a(24.0f, 24.0f);
    const ImVec2 b(a.x + paint::Span(size, line.c_str()) + 16.0f, a.y + size + 16.0f);
    paint::Box(sheet, a, b, paint::ink().glass);
    paint::Corners(sheet, a, b, paint::ink().amber, 8.0f, 1.0f);
    paint::Text(sheet, ImVec2(a.x + 8.0f, a.y + 8.0f), size, paint::ink().text, line.c_str());
}
```

## `zsdk/paths.h`

The game's folders, as pure functions of two roots: the exe path and `%LOCALAPPDATA%`.

**Thread:** any. Only `SaveGamesData` reads the disk (a folder list).

**Stability:** Stable

| Call | What it gives |
|---|---|
| `Current()` | the `Roots` of this process: `exe` (the game exe path) and `local` (`%LOCALAPPDATA%`) |
| `GameRoot(roots)` | the game folder above `Binaries\Win64`, or empty when the exe is not in such a folder |
| `Mods(roots)` | `<game>\Content\Paks\~mods` |
| `Saved(roots)` | `%LOCALAPPDATA%\Stalker2\Saved` |
| `Logs(roots)` | `Saved\Logs` |
| `AppliedSettings(roots)` | `Saved\GameSettings\AppliedSettingsWin64.cfg` |
| `SteamManifest(roots)` | `steamapps\appmanifest_1643320.acf`, when the game sits in `steamapps\common` |
| `SaveGamesData(roots)` | the existing `Saved\<folder>\SaveGames\Data` folders, sorted, at most 64 |
| `PlayerSave(file)` | the file is a player save (`.sav` in `SaveGames\Data`, not `CampaignsSave` or `AnalyticsData`) |

Each call except `PlayerSave` also has a plain form without `roots` that uses `Current()`. On store builds whose exe is not in `Binaries\Win64` (such as the Game Pass build), `GameRoot`, `Mods` and `SteamManifest` are empty; ZoneSDK supports the Steam build only. In a test program with `ZSDK_TEST_HOME` set, `Current()` points into that folder, so no test reads the real game or user folders.

<!-- doccheck: present body -->
```cpp
const zsdk::paths::Roots roots = zsdk::paths::Current();
zsdk::Log("game %s, mods %s", zsdk::log::Shown(zsdk::paths::GameRoot(roots)).c_str(), zsdk::log::Shown(zsdk::paths::Mods(roots)).c_str());
for (const std::filesystem::path &folder : zsdk::paths::SaveGamesData(roots))
    zsdk::Note(2, "save folder %s", zsdk::log::Shown(folder).c_str());
```

## `zsdk/policy.h`

Behaviour switches of the SDK, set once in `App::policy`. New add-ons keep `zsdk::Policy{}`.

**Thread:** any (the policy is part of the constant `App`).

**Stability:** Stable

**Legacy, may be removed in 2.0:** `Policy::marks`, `Policy::gone`, `Policy::spelling`, `Policy::widen`, `Policy::census`, `Policy::knock`, `Policy::knockMs`, `Policy::idleMs`, `Policy::wire`, `Policy::wireMs` and their enums `Marks`, `GoneFlags`, `Spelling`, `CensusMode`, `Knock`, `WireRetry`.

| `Policy` field | Default | What it changes |
|---|---|---|
| `strictMarks` | `true` | a marked call whose crash marker cannot be written is refused |
| `verifiedOnly` | `true` | unverified verbs, doomed targets and doomed object parameters are refused before the call |
| `halt` | `Halt::Key` | `Key`: a native fault disables only its own key; `Session`: it also stops every call for the session |
| `haltOnSeh` | `true` | a structured exception that escapes a game-thread tick stops every call for the session |
| `watchKept` | 8 | the most crash windows open at once, 1 to 32 (`ledger::WatchLimit`) |
| `files` | `FileThread::Render` | log and settings files are written from the present tick; `Render` is the only value |
| `mods` | `ModRule::Alt` | `Alt`: a plain key fires with Ctrl or Shift held; `Exact`: the modifiers must be equal |
| `marks` | `Marks::All` | `All`: every `Fire` is marked; `Flagged`: only `Risky` rows and `Fire(call, key)` |
| `gone` | `GoneFlags::Strict` | the flag mask `ue::Gone` uses: `Strict` (`kGoneStrict`) or `Legacy` (`kGoneLegacy`) |
| `spelling` | `Spelling::Tolerant` | `Tolerant`: a parameter spelled with other letter case is accepted with a log line; `Strict` |
| `widen` | `false` | a missing function is also looked up on the target's class |
| `census` | `CensusMode::Interval` | `Interval`: sweeps on a timer; `OnDemand`: sweeps when `census::Want` asks |
| `knock`, `knockMs`, `idleMs` | `Knock::BusyIdle`, 100, 1000 | how often the game thread is woken while the add-on is not hot and no request waits: `BusyIdle` every `idleMs`, `Fixed` every `knockMs`, `Configured` as `thread::Knock` sets; while hot or while a request waits it is woken at every frame |
| `wire`, `wireMs` | `WireRetry::Backoff`, 2000 | engine wiring retries: `Backoff` every `wireMs` for 5 tries, then every 30 s; `Fixed` every `wireMs`; 10 tries at most |

`Policy` values compare with `==`. `Legacy(policy)` is true when any field but `watchKept` differs from the default; `LegacyText(policy)` lists those fields (`marks=flagged, knock=fixed 100/1000`) or says `none`. The boot log and Diagnostics show the list.

<!-- doccheck: present body -->
```cpp
const zsdk::App &app = zsdk::app::Get();
if (zsdk::Legacy(app.policy)) zsdk::Log("legacy rules: %s", zsdk::LegacyText(app.policy).c_str());
constexpr zsdk::Policy kStrict{ .halt = zsdk::Halt::Session, .watchKept = 16 };
static_assert(kStrict.marks == zsdk::Marks::All && kStrict.mods == zsdk::ModRule::Alt && kStrict.files == zsdk::FileThread::Render);
zsdk::Note(2, "this add-on runs the default policy: %s", app.policy == zsdk::Policy{} ? "yes" : "no");
```

## `zsdk/present.h`

The first include of every present unit. It includes `zsdk/zsdk.h` and stops the build (`#error`) when ImGui is visible in the unit or the unit is not built with `IMGUI_DISABLE`: ImGui code belongs in `src/overlay`.

**Thread:** none (an include).

**Stability:** Stable

<!-- doccheck: present file -->
```cpp
#include <zsdk/present.h>

#include "template.h"

namespace ztemplate::core
{
    const char *Hello()
    {
        return kVersion;
    }
}
```

## `zsdk/registry.h`

The SDK's table of released add-ons with their tags, toast lanes and default keys. An add-on brings its own row in `App::row`; the table is a cross-check against clashes.

**Thread:** any.

**Stability:** Provisional

A `Row` has `id`, `tag`, `lane` (the toast lane as a fraction of the screen height, 0 for no toasts) and `keys` (default chords separated by `|`).

| Call | What it does |
|---|---|
| `Rows()` | every row of the table |
| `Find(id)` | the row of an id, or `nullptr` |
| `Lane(id, fallback)` | the lane of an id when it has one above 0, else `fallback` |
| `kSpareLane` | 0.75, the lane of an add-on without a row |

The generic `identity` suite fails when another table row uses the add-on's tag, and warns when its id is not in the table or its lane is shared.

<!-- doccheck: present body -->
```cpp
const zsdk::registry::Row *row = zsdk::app::Row();
const zsdk::registry::Row *listed = row ? zsdk::registry::Find(row->id) : nullptr;
if (row && !listed) zsdk::Note(1, "%s is not in the ZoneSDK table, its own row is used", row->id);
const float lane = zsdk::registry::Lane(row ? row->id : "", zsdk::registry::kSpareLane);
zsdk::Note(2, "toast lane %.2f, %zu rows in the table", static_cast<double>(lane), zsdk::registry::Rows().size());
```

## `zsdk/report.h`

The text behind "Скопіювати звіт / Copy report": versions, the game build, the ReShade version, the add-ons next to the game exe, the crash ledger, the add-on's own lines and the end of the log, with user paths hidden.

**Thread:** overlay (Diagnostics builds it there), or any thread but D3D and loader: `Text` flushes the log and reads files.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Text(reshade)` | the report; `reshade` is the ReShade module, for its version (`nullptr` gives `unknown`) |
| `Version(module)` | the file version of a loaded module, `a.b.c.d` or `unknown` |
| `File()` | `report.txt` in the data folder, or empty |
| `Save(text)` | writes the report to `File()` |
| `kLogLines` | 80, the log lines in the report |
| `kTailBytes` | 64 KiB, the most of the log read for them |

`hooks.report` adds the add-on's own lines under a heading with its name. The HelloHud sample's report hook, in the template's terms:

<!-- doccheck: present -->
```cpp
std::string Section()
{
    const Status s = core::Snapshot();
    return std::string("template: located ") + (s.located.empty() ? "-" : s.located) + ", refusal " + (s.refusal.empty() ? "-" : s.refusal);
}
```

## `zsdk/shell.h`

The add-on's entry: the exports ReShade reads and the `DllMain` hand-over. Include it only in `src/entry.cpp`.

**Thread:** `Main`: loader (`DllMain`). `Module`, `Stopping`, `Ready` and `Exiting`: any.

**Stability:** Stable

| Name | What it does |
|---|---|
| `ZSDK_ADDON(name, description)` | exports `NAME` and `DESCRIPTION` for ReShade; once per add-on |
| `Main(module, reason, reserved, hooks)` | everything `DllMain` does: on attach it checks the game exe and the unique tag, registers with ReShade and binds the hooks; on detach it stops the SDK and calls `hooks.stop` inside the fence |
| `Module()` | the add-on's module handle |
| `Ready()` | true once `hooks.boot` has returned at the first present; stays false after a fault in the boot |
| `Stopping()` | true once the add-on unloads |
| `Exiting()` | true when the process exits (detach with a non-null `reserved`); every other thread is gone |

The generic `hardrules` suite wants exactly one `ZSDK_ADDON` and one `DllMain` that only returns `zsdk::shell::Main(...)`. The template's `src/entry.cpp`, after the hooks table:

<!-- doccheck: entry -->
```cpp
ZSDK_ADDON("ZoneTemplate", TEMPLATE_DESCRIPTION)

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    static const zsdk::Hooks hooks = Wire();
    return zsdk::shell::Main(module, reason, reserved, hooks);
}
```

## `zsdk/text.h`

UTF-8 text helpers that know Cyrillic.

**Thread:** any. `Read` and `Write` touch the disk.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Trim(raw)` | without spaces, tabs and line ends at both ends |
| `Split(line, mark)` | parts between `mark` characters |
| `Tokens(line)` | words separated by spaces, tabs, commas or line ends; double quotes keep separators inside a word |
| `Lines(body)` | lines without line ends, a BOM skipped |
| `Wide(utf8)`, `Narrow(wide)` | UTF-8 to UTF-16 and back |
| `Fold(point)`, `Raise(point)` | one code point to lower or upper case, Latin and Cyrillic |
| `Lower(utf8)`, `Upper(utf8)` | a text to lower or upper case |
| `Latin(utf8)` | a loose Latin search key of a Latin or Cyrillic text, so `Zaton` and `Затон` give the same key |
| `Cyrillic(utf8)` | the text has a Cyrillic letter |
| `Points(utf8)` | the code points |
| `Starts(text, head)`, `Ends(text, tail)` | prefix and suffix tests |
| `Same(left, right)` | equal, ignoring ASCII letter case |
| `Color(token, argb)` | `#RRGGBB` to an opaque ARGB value |
| `Format(shape, ...)` | `printf` into a `std::string` |
| `Read(path)`, `Write(path, body)` | a whole file as bytes |

<!-- doccheck: present body -->
```cpp
const std::vector<std::string> parts = zsdk::text::Split("ALT+K|F7", '|');
const std::string lower = zsdk::text::Lower("Зона ВІДЧУЖЕННЯ");
uint32_t argb = 0;
const bool read = zsdk::text::Color("#D9A227", argb);
zsdk::Note(2, "%zu keys, %s, %zu wide chars, color %08X %s", parts.size(), lower.c_str(), zsdk::text::Wide(lower).size(), argb, read ? "read" : "bad");
```

## `zsdk/thread.h`

The game-thread pump and the add-on's phase. The SDK hooks the game thread's message queue and runs its pump there when it wakes it; every game call happens inside that pump.

**Thread:** any.

**Stability:** Stable

**Internal:** `thread::Booked`, `thread::Rejected` (worker states; Diagnostics and the report show them).

`Phase` is what the tab shows; `thread::State(stable)` gives it, with `stable` from the world grip:

| `Phase` value | Meaning |
|---|---|
| `Off` | the SDK has not started yet |
| `Hooking` | waiting for the game window to hook its thread |
| `Binding` | looking for the engine in the exe |
| `Wiring` | preparing the first game call |
| `Waiting` | waiting for the game world; `Grip::wait` says what for |
| `Ready` | game calls work |
| `Failed` | this game build or exe is not supported, or wiring gave up |
| `Halted` | game calls are stopped for this session after a fault |
| `Lost` | the game thread has not answered for over 2 s while frames are drawn |
| `Count` | the number of phases |

| Call | What it does |
|---|---|
| `Ask(order)` | queues a request for `hooks.order` on the game thread and wakes it; at most 8 wait, the oldest is dropped |
| `Wake()` | wakes the game thread for a pump |
| `Knock(ms)` | the wake period under the Legacy `Knock::Configured` |
| `OnGame()` | this is the game thread |
| `Started()`, `Hooked()`, `Bound()`, `Wired()` | the SDK started, hooked the game thread, found the engine, wired game calls |
| `Halted()` | game calls are stopped for this session |
| `Stopping()` | the SDK stops or the game asked to exit |
| `Overlaid()` | the ReShade overlay is open |
| `LastMs()`, `WorstMs()` | how long the last pump took, and the worst in the last 10 s |
| `LastPumpAt()`, `PumpAlive()` | when the last pump ran, and whether one ran in the last 2 s |
| `Count()` | the pump `Counters` |
| `State(stable)` | the `Phase` |

| `Counters` field | What it counts |
|---|---|
| `posted`, `seen`, `ticked` | wake messages posted, seen by the hook, pumps run |
| `pumpAt` | when the last pump ran |
| `turns` | turns of the SDK worker loop |
| `wires`, `wireMs` | wiring attempts and the cost of the last one |

The template's tab and key code use it like this:

<!-- doccheck: present body -->
```cpp
const zsdk::world::Grip w = zsdk::world::Snapshot();
const zsdk::Phase phase = zsdk::thread::State(w.stable);
if (phase == zsdk::Phase::Ready && !zsdk::thread::Overlaid()) zsdk::thread::Ask(static_cast<int>(Order::Locate));
const zsdk::thread::Counters c = zsdk::thread::Count();
zsdk::Note(2, "pump: posted %llu, ticked %llu, last %.2f ms, alive %d", static_cast<unsigned long long>(c.posted), static_cast<unsigned long long>(c.ticked), zsdk::thread::LastMs(), zsdk::thread::PumpAlive() ? 1 : 0);
```

## `zsdk/toast.h`

Short on-screen notes. The add-on queues them from any thread; the SDK draws them in its toast lane (`app::Lane()`) with the add-on's `hooks.words` and `hooks.tone`.

**Thread:** any (one lock). Drawing happens in the overlay event.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Push(kind, detail, args, replaceKind)` | queues a toast; `replaceKind` first drops the newest toasts of the same kind, so a repeated note does not pile up |
| `Live(now)` | the toasts to show now, at most `kShown` |
| `Reset()` | drops every toast; the SDK does when ReShade's runtime goes |
| `Focused()` | the game window has the focus; toasts are drawn only then |
| `kLifeMs`, `kFadeMs` | 3000 ms on screen, the last 400 ms fading |
| `kShown`, `kKept` | 3 shown at once, 8 queued |
| `kUnsupported` | -1, the SDK's own "this game build is not supported" toast |

An `Item` has the add-on's `kind` (0 and above; kinds below 0 are the SDK's), a free `detail` text, the `args` for `hooks.words` and `born` (when it was pushed). The template's `Toast` and the words for it:

<!-- doccheck: present -->
```cpp
void Toast(Say say, const std::string &arg)
{
    zsdk::toast::Push(static_cast<int>(say), {}, { arg }, false);
}
```

<!-- doccheck: overlay -->
```cpp
std::string Words(const zsdk::toast::Item &item, zsdk::lang::Lang ui)
{
    const char *id = item.kind == static_cast<int>(Say::Located) ? "say.located" : "say.refused";
    return zsdk::lang::Format(zsdk::lang::Text(id, ui), { item.args.empty() ? std::string() : item.args.front() });
}
```

## `zsdk/ue.h`

Read-only access to the game's objects and reflection: the object array, names, classes, properties and functions. Every read goes through a safe copy that never faults.

**Thread:** any for single reads, but an object can die at any time on another thread, so decide on the game thread. `FindClass`, `FindLive` and `Host` walk the whole object array: game or worker, never at every frame. `Function`, `Functions`, `Analyze` and `Choices`: any.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Here(thing, type)` | the object lives in the object array at its own index (and has class `type` when given); check every pointer you keep with it |
| `Doomed(thing)` | the object is not `Here`, or is being destroyed (object flags `kObjectDoomed`, or item flags `kItemGarbage` or `kItemUnreachable`); the game destroys objects over several frames, so an object can be `Here` and doomed |
| `Gone(thing)` | the object carries the gone flags of the policy (`kGoneStrict` or `kGoneLegacy`) |
| `Standing(thing, type)` | the object is in the array with exactly this class |
| `Seal(thing)`, `Ours(mark)` | an identity `Mark` of an object, and whether a pointer is still that same object and not doomed |
| `Look(thing)` | a log text with the object's flags and verdict |
| `ClassOf(thing)`, `OuterOf(thing)` | an object's class and outer |
| `Label(thing)`, `Path(thing)` | an object's name with its number suffix, and its outer chain |
| `Kin(type, base)`, `Inherits(type, name)` | a class inherits another class, or a class of that name |
| `Flags(thing)` | the object flags |
| `PropertyOffset(type, field)` | the offset of a reflected property, or -1 |
| `Bulk(type)` | the size of a struct or class |
| `FindClass(name)` | a class by name |
| `FindLive(baseClass)` | the newest object (not a class default) whose class inherits `baseClass` |
| `Host(type)` | an object to run calls of `type` on; class defaults only for function libraries |
| `Function(owner, name)`, `Functions(name)` | a `UFunction` from the function book, or all functions of a name |
| `Analyze(function, out)` | a function's parameters as a `Sign` |
| `Choices(param, out)`, `Choose(picks, word, out)` | the entries of an enum parameter as `Pick`s, and the value of one entry by name |
| `CopyText(at)` | an `FString` in game memory as `std::wstring`, without freeing it |
| `Lend(text)` | a `Lent` view of a string for an `FString` parameter; the string must outlive `Fire` |
| `NameText(index, number)`, `Word(slot)` | an `FName` as text |
| `Copy(at, into, bytes)`, `Read<T>(at, out)`, `Held(spot, bytes)` | safe reads; whether a memory range is readable |
| `Grab(thing, at)` | the pointer stored at an offset |
| `Total()`, `Stride()`, `Item(index)`, `At(index)`, `Serial(index)` | the object array: count, item size, item, object and serial number at an index |
| `Ready()`, `Bound()` | game calls are wired; the engine is found |

`Lend` has two forms, for `std::wstring` and for `const wchar_t *`; a temporary `std::wstring` does not compile (`Lend(const std::wstring &&) = delete`), and a text of `kLendCap` (1 Mi) characters or more gives an empty view. A `Lent` is `data`, `num` (the length with the closing zero) and `max`.

| Type | Fields |
|---|---|
| `Param` | `name`, `type` (the property class, such as `IntProperty`), `field`, `at`, `size`, `flags` |
| `Sign` | `owner`, `bytes` (the parameter block size), `params` |
| `Pick` | `name`, `value` |
| `Mark` | `actor`, `type`, `index`, `name`, `serial` |

The layout constants are `kIndexAt`, `kClassAt`, `kNameAt`, `kOuterAt` and `kSuperAt` (object and class fields), `kItemFlagsAt` and `kItemClusterAt` (object array item fields), and the flag masks `kGoneStrict`, `kGoneLegacy`, `kObjectDoomed`, `kItemGarbage` and `kItemUnreachable`. Keep objects through the game's own references and check them with `Ours` or `Doomed` before each use; never set root bits ([RULES.md](RULES.md#7-game-objects-and-game-state)).

<!-- doccheck: present body -->
```cpp
const zsdk::world::Grip &w = zsdk::world::Hold();
if (!zsdk::ue::Here(w.body, w.bodyType) || zsdk::ue::Doomed(w.body)) return;
static zsdk::ue::Mark kept{};
if (!zsdk::ue::Ours(kept)) kept = zsdk::ue::Seal(w.body);
zsdk::Note(2, "pawn %s of class %s: %s", zsdk::ue::Path(w.body).c_str(), zsdk::ue::Label(zsdk::ue::ClassOf(w.body)).c_str(), zsdk::ue::Look(w.body).c_str());
```

## `zsdk/verbs.h`

Verb tables: the game functions an add-on calls, with the parameters it expects. The SDK checks every row against the game's reflection once, at prime, and an unverified row is never called.

**Thread:** `Prime`: game (the SDK runs it). `Last`, `Ready`, `Note` and `Primed`: any. The other calls are pure.

**Stability:** Stable

| `Spec` field | What it holds |
|---|---|
| `owner`, `name` | the function's owner class and name |
| `required` | the add-on's core needs it; `Report::coreReady` is false while a required row fails |
| `params` | up to 12 `Want`s |
| `mark` | `Mark::Auto`, or `Mark::Risky` to always run it behind a crash marker |
| `count` | the exact number of parameters, or -1 |
| `bytes` | the exact parameter block size, or -1 |

A `Want` is one expected parameter: `name`, `types` (property class names separated by `|`, or `nullptr` for any), `at` (offset, -1 for any) and `size` (0 or less for any). `Check` is a row's verdict: `Unknown`, `Ok` or `Bad`.

| Call | What it does |
|---|---|
| `Prime(specs)` | checks a table and returns a `Report` |
| `Last()` | the last `Report`: `requiredOk`, `requiredAll`, `optionalOk`, `optionalAll`, `missing` (the failed rows) and `coreReady` |
| `Ready(verb)` | the row passed |
| `Note(verb)` | what the check found for the row |
| `Primed()` | the table was checked |
| `Key(owner, name)`, `KeyOf(spec)` | the ledger key `Owner.Name` |
| `Verify(sign, missing, found, spec, tolerant, note, spelled)` | the check of one row, for tests |
| `TypeFits(type, types)` | a property class is in a `|` list |
| `Same(a, b)` | `constexpr` string equality |
| `Counted(specs, required)` | `constexpr` count of required or optional rows |

The template's prime hook:

<!-- doccheck: present body -->
```cpp
const zsdk::verbs::Report r = zsdk::verbs::Last();
if (!r.coreReady) zsdk::Log("template: a required game function is missing: %s", r.missing.c_str());
if (zsdk::verbs::Primed() && !zsdk::verbs::Ready(kLocation))
    zsdk::Log("template: %s: %s", zsdk::verbs::KeyOf(kSpecs[kLocation]).c_str(), zsdk::verbs::Note(kLocation).c_str());
```

## `zsdk/verbs/common.h`

The shared verb catalog and the table helpers: the world verbs every add-on needs, the screen checks, two console commands, and `Join` and `Index` to build a table at compile time.

**Thread:** none (compile-time data and `constexpr` functions).

**Stability:** Stable

**Provisional:** `common::kScreen` and its `Screen` values (the screen checks may still change in a minor release).

| Name | What it is |
|---|---|
| `Join(a, b)` | a table made of two tables (C arrays or a `std::array` and a C array); `constexpr` |
| `Index(specs, owner, name)`, `Index(specs, name)` | the index of a row, or -1; `constexpr` |
| `Counted(specs, required)` | the `std::array` form of `verbs::Counted` |
| `common::kWorld` | the six world verbs, indexed by the `World` values `kPlayerController`, `kPlayerPawn`, `kCameraManager`, `kTimeSeconds`, `kGamePaused` (optional), `kFrameCount`, with `kWorldCount` 6 |
| `common::Plays(specs, verb, role)` | row `verb` of `specs` is the catalog verb of a `World` role |
| `common::kStatics` | `GameplayStatics` and `KismetSystemLibrary`, for `CensusSpec::statics` |
| `common::kScreen` | optional screen checks, indexed by the `Screen` values `kInCinematic`, `kInStaticDialog`, `kUsingPda`, `kUsingBackpack`, `kInInteraction`, with `kScreenCount` |
| `common::kConsole` | two console commands of `kConsoleHost` (`CustomConsoleManagerAI2`), indexed by the `Console` values `kDestroyModel`, `kNeedsValue`, with `kConsoleCount`; optional and `Risky`; list `kConsoleHost` in `CensusSpec::hosts` |
| `kObject`, `kInt`, `kFloat`, `kReal`, `kFrame`, `kBool`, `kStr`, `kEnum`, `kStruct` | property type lists for `Want::types` |

Keep `common::kWorld` at the head of the table, so the `WorldVerbs` defaults fit, and the add-on's own rows after it. The template's `src/template.h`:

<!-- doccheck: present -->
```cpp
namespace common = zsdk::verbs::common;
inline constexpr zsdk::verbs::Spec kOwn[] = {
    { .owner = "Actor", .name = "K2_GetActorLocation", .required = true, .params = { { .name = "ReturnValue", .types = common::kStruct, .at = 0, .size = 24 } }, .mark = zsdk::verbs::Mark::Risky },
};
inline constexpr auto kSpecs = zsdk::verbs::Join(common::kWorld, kOwn);
enum Verb : int { kLocation = common::kWorldCount, kVerbs };
static_assert(kSpecs.size() == static_cast<size_t>(kVerbs) && zsdk::verbs::Counted(kSpecs, true) == 6);
static_assert(zsdk::verbs::Index(kSpecs, "Actor", "K2_GetActorLocation") == kLocation && zsdk::verbs::Index(kSpecs, "GetFrameCount") == common::kFrameCount);
```

## `zsdk/version.h`

The ZoneSDK version and the pinned third-party versions.

**Thread:** none (macros).

**Stability:** Stable

| Macro | What it is |
|---|---|
| `ZSDK_VERSION_MAJOR`, `ZSDK_VERSION_MINOR`, `ZSDK_VERSION_PATCH` | the version numbers |
| `ZSDK_VERSION` | the version as text, for example `"1.0.0"` |
| `ZSDK_VERSION_NUMBER` | the version as `1,0,0,0`, for a version resource |
| `ZSDK_TEXT(value)` | a macro value as text |
| `ZSDK_RESHADE_API` | 20, the ReShade API of the bundled headers (ReShade 6.8) |
| `ZSDK_IMGUI_NUM` | the bundled Dear ImGui version number |

The boot log line names the ZoneSDK version and the first 8 digits of the release manifest.

<!-- doccheck: present body -->
```cpp
static_assert(ZSDK_RESHADE_API == 20, "this add-on is built for ReShade API 20");
const char *api = ZSDK_TEXT(ZSDK_RESHADE_API);
zsdk::Log("built on ZoneSDK %s (%d.%d.%d), ReShade API %s, ImGui %d", ZSDK_VERSION, ZSDK_VERSION_MAJOR, ZSDK_VERSION_MINOR, ZSDK_VERSION_PATCH, api, ZSDK_IMGUI_NUM);
```

## `zsdk/world.h`

The player's world as the SDK watches it: the game instance, the player controller, pawn and camera, the world, its clock, and whether it is stable enough to call into.

**Thread:** `Hold`, `WorldOf`, `OwningWorld`, `Look`, `PovAt`, `OpenWorld`: game. `Player`, `Clock` and `Paused(context)`: game, they call the game. `Snapshot`, `Paused()` and `Epoch`: any.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Hold()` | the live `Grip` on the game thread; on another thread it logs once and gives that thread's own copy of `Snapshot()`, valid until the next `Hold()` on that thread |
| `Snapshot()` | a copy of the grip from the last publish, for other threads |
| `Paused()` | the game is paused |
| `Epoch()` | the world epoch of the last publish |
| `WorldOf(thing)` | the world an object belongs to; on the open-world map that is the streamed cell's world |
| `OwningWorld(thing)` | the persistent world, through the object's level; `nullptr` while an object on the way is doomed or loading |
| `Look(lens, type, at, loc, rot)` | the camera's view point at offset `at` of the camera manager, checked for sane values |
| `PovAt(lens, type)` | that offset for a camera manager class, or -1 |
| `Player(verb, context)` | runs a player getter of the world verbs (controller, pawn or camera) |
| `Clock(context)` | the world time in seconds |
| `OpenWorld()` | the grip's world is the open-world map (`WorldVerbs::map`) |
| `Paused(context)` | asks the game whether it is paused |

The SDK watches the world every 500 ms on the game thread. The world is `stable` when the controller, pawn and world are there on the open-world map for 3 s with a running clock; a new world or a clock that runs back (a loaded save) is a world change: the epoch goes up, `hooks.turn` runs, and the grip is unstable again.

| `Grip` field | What it holds |
|---|---|
| `gi`, `giType` | the game instance and its class |
| `pilot` | the player controller |
| `body`, `bodyType` | the player pawn and its class |
| `lens`, `lensType` | the player camera manager and its class |
| `world`, `worldType`, `worldName` | the pawn's world, its class and its name |
| `home` | the world the epoch belongs to |
| `povAt` | the camera view point offset, or -1 |
| `seconds`, `frame` | the world time and the engine frame counter, -1 when unknown |
| `frameless` | there is no frame counter, so steps are not one per frame |
| `growing` | the world clock moved since the last watch |
| `fit` | controller, pawn and world are there on the open-world map |
| `stable` | fit for 3 s with a running clock |
| `paused` | the game is paused |
| `first` | no world was seen yet |
| `since` | when the last world change or pawn change happened |
| `polled`, `probed` | when the world and the game instances were last checked |
| `epoch` | the world epoch, +1 at every world change |
| `wait` | the language row id of what the grip waits for (`wait.world`, `wait.settle`, `wait.consoles`) or empty |
| `at` | when `Snapshot()`'s copy was taken |

A `Turn` tells `hooks.turn` about a world change: `why` (a text for the log), `world`, `sameWorld` (a save loaded into the same map) and `epoch`. The template's publish and the HelloHud sample's hot and turn hooks:

<!-- doccheck: present -->
```cpp
bool Hot()
{
    return zsdk::world::Hold().stable;
}

void Turn(const zsdk::world::Turn &turn)
{
    zsdk::Note(1, "template: world turn %u (%s)%s", turn.epoch, turn.why, turn.sameWorld ? ", same world" : "");
}

void Publish(uint64_t)
{
    const zsdk::world::Grip &w = zsdk::world::Hold();
    zsdk::Note(2, "template: world %s, stable %d, waits for %s", w.worldName.c_str(), w.stable ? 1 : 0, w.wait && *w.wait ? w.wait : "-");
}
```

## `zsdk/zsdk.h`

The umbrella header: everything a present unit may use. It includes `app.h`, `base.h`, `call.h`, `census.h`, `exe.h`, `glyphs.h`, `guard.h`, `hooks.h`, `ini.h`, `input.h`, `job.h`, `keys.h`, `lang.h`, `ledger.h`, `log.h`, `math.h`, `mem.h`, `paths.h`, `policy.h`, `registry.h`, `report.h`, `text.h`, `thread.h`, `toast.h`, `ue.h`, `verbs.h`, `version.h` and `world.h` (and `verbs/common.h` through `app.h`). It does not include `overlay.h`, `paint.h`, `shell.h` or `present.h`.

**Thread:** none (an include).

**Stability:** Stable

The add-on's own header includes it, as the template's `src/template.h` does after `#pragma once`:

<!-- doccheck: present file -->
```cpp
#include "version.h"

#include <zsdk/zsdk.h>

namespace ztemplate
{
    std::string Versions()
    {
        return std::string("ZoneSDK ") + ZSDK_VERSION + ", add-on " + TEMPLATE_VERSION;
    }
}
```

## `zsdk/test/addon.h`

The add-on's source tree, as the generic suites see it: which files are present units, overlay units, the entry and the tests, and the `tests/zsdk.cfg` keys.

**Thread:** test programs only (`tests/*.cpp`), never add-on sources.

**Stability:** Stable

| Name | What it does |
|---|---|
| `Load()` | reads the tree of the add-on under test (`ZSDK_ADDON_SOURCE`) into a `Tree` |
| `Tree` | `root`, `sdk` (the release, `ZSDK_HOME`), `cfg` (the `tests/zsdk.cfg` keys), `present`, `overlay`, `entry` and `tests` |
| `Allowed(tree, key)` | a comma list from `zsdk.cfg` as a set, for example `present.allow` |
| `Rel(tree, file)` | a file path relative to the add-on, with forward slashes |
| `Under(file, dir)` | the file lies inside the folder |
| `GuardSets(body)`, `GuardSets(tree)` | the `guard::Set` declarations in a text or in the whole tree, each a `Declared` with `id`, `name` and `file`; the name is the first value in the braces when it is a string literal, or a designated `.name = "..."` |

The `zsdk.cfg` keys (`name`, `define`, `registry`, `banned`, `facade`, `memory`, `present.allow`, `identity.same`, `workspace`, `comments`) are described in [RULES.md](RULES.md#zsdkcfg). A local suite in the style of the generic ones:

<!-- doccheck: test -->
```cpp
using check::That;
const addon::Tree t = addon::Load();
That(!t.present.empty() && !t.entry.empty(), "the add-on has present units and src/entry.cpp");
for (const auto &file : t.present)
    That(scan::Lex(scan::Body(file)).closed, "every literal is closed in " + addon::Rel(t, file));
const std::set<std::string> allowed = addon::Allowed(t, "present.allow");
That(allowed.size() < 100, "the present.allow list is short, " + std::to_string(allowed.size()));
```

## `zsdk/test/check.h`

The checks of every test program.

**Thread:** test programs only.

**Stability:** Stable

| Call | What it does |
|---|---|
| `That(ok, what)` | one check; prints `FAIL <what>` when it fails |
| `Warn(what)` | prints `warning: <what>` and counts it; the build summary counts the warnings |
| `Skip(what)` | a warning that says `SKIPPED` |
| `Finish(name)` | prints `<name>: N checks, M failures` (and the warnings) and gives the exit code; for a failed `zsdk-*` suite it adds a line that points to its part of RULES.md, section 10; `return check::Finish("local");` ends `main` |
| `Count()`, `Failures()`, `Warnings()` | the counters |

The build runs every `tests/*.cpp` as its own program with `ZSDK_TESTS` and `<DEFINE>_TESTS` defined, in a sandbox folder (`ZSDK_TEST_HOME`); `ZSDK_ADDON_NAME`, `ZSDK_ADDON_SOURCE`, `ZSDK_HOME` and `ZSDK_SYMBOLS` tell it what it tests. The template's `tests/local.cpp`:

<!-- doccheck: test -->
```cpp
using check::That;
ztemplate::Settings s;
s.language = "en";
s.locate = zsdk::keys::Parse("ALT+K");
const std::string text = zsdk::ini::Text(s);
ztemplate::Settings back;
zsdk::ini::Parse(text, back);
That(back == s, "the template settings survive a round trip");
That(text.find("[keys]\r\nlocate=ALT+K\r\n") != std::string::npos, "the locate key is written by name");
```

## `zsdk/test/scan.h`

A small C++ source scanner and file helpers for test programs: it removes comments and string literals, finds function bodies, calls and token patterns.

**Thread:** test programs only.

**Stability:** Stable

| Call | What it does |
|---|---|
| `Lex(body)` | a `Lexed`: `code` (comments removed, literals emptied), `literals`, `words`, `comments` (how many), `firstComment` (its line) and `closed` (every literal is closed) |
| `Functions(code)` | the function bodies of lexed code, each a `Function` with `name`, `at` and `body` |
| `Calls(code)` | the names called in lexed code |
| `Tokens(code)` | the code split into tokens (`::` is one token) |
| `Rule(rule)`, `Seek(tokens, rule)`, `Uses(tokens, rule)` | a token pattern such as `"zsdk :: Fire ("`, where `word*` matches a token prefix (each a `Piece` with `text` and `prefix`): parse it, find its position, test it |
| `Ticks(code)` | raw tick differences such as `now - at`, each a `Tick` with `line` and `text` |
| `Body(file)`, `Files(dir, kinds, deep)`, `Dirs(dir)` | read a file; list files of some extensions; list folders |
| `Text(path)`, `Generic(path)` | a path as UTF-8, with forward slashes for `Generic` |
| `Env(name, fallback)`, `EnvPath(name, fallback)`, `Wide(name)` | an environment variable as text, as a path (safe for any letters), as UTF-16 |
| `AddonRoot()`, `SdkRoot()` | `ZSDK_ADDON_SOURCE` and `ZSDK_HOME` |
| `Config(file)`, `List(csv)`, `Trim(raw)` | read `key=value` lines; split a comma list; trim |
| `Ident(c)`, `LineOf(body, at)`, `Close(code, open)`, `Before(code, at)`, `Count(text, what)` | small helpers: an identifier character, the line of an offset, the matching `}`, the word before an offset, how often a text occurs |

<!-- doccheck: test -->
```cpp
using check::That;
int found = 0;
for (const auto &file : scan::Files(scan::AddonRoot() / "src", { ".cpp" }, true))
    for (const scan::Function &f : scan::Functions(scan::Lex(scan::Body(file)).code)) found += f.name == "Locate" ? 1 : 0;
That(found >= 1, "the add-on defines Locate");
const std::vector<std::string> tokens = scan::Tokens(scan::Lex("zsdk::Fire(c, key);").code);
That(scan::Uses(tokens, "zsdk :: Fire ("), "the token rule finds the call");
```

## `zsdk/test/hardrules.h`

The rule table of the generic `hardrules` suite (the calls add-on code may not use and the hints it prints).

**Thread:** test programs only.

**Stability:** Internal

Add-ons do not include it. [RULES.md](RULES.md#zsdk-hardrules) lists what the suite refuses.

## `zsdk/test/ledger_raw.h`

The raw crash ledger (arm, prove, settle, disable, the exit mark) for the SDK's own test suites. It stops the build (`#error`) without `ZSDK_TESTS`, so no add-on unit reaches it.

**Thread:** SDK test programs only.

**Stability:** Internal

Add-ons do not include it: they use `ledger::Window`, `ledger::Blocked` and `zsdk::Fire`, which run the ledger for them.
