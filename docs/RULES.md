# ZoneSDK rules

These are the rules for ReShade add-ons built on ZoneSDK 1.0. Most of them are not advice: the compiler, the SDK at run time, or the test suites that run on every `build.cmd -Tests` enforce them. This page says what each rule is, why it exists and which check catches it.

When a generic test fails, the build prints the failure and then the suite, for example:

```
FAIL no std::thread in src/scan.cpp: background work runs through zsdk::job::Every on the SDK worker, never on an own thread, timer or pool callback
zsdk-hardrules: 303 checks, 1 failures
zsdk-hardrules: docs\RULES.md (docs\RULES-UA.md in Ukrainian), section 10, zsdk-hardrules, gives the rule behind each message of this suite
FAIL zsdk-hardrules (1)
```

Look up the suite name (`zsdk-hardrules`) in [section 10](#10-the-generic-test-suites), as the line under the count says: it lists every message of that suite and the rule behind it.

Ukrainian version: [RULES-UA.md](RULES-UA.md). See also [GETTING_STARTED.md](GETTING_STARTED.md), [API.md](API.md), [VERSIONING.md](VERSIONING.md) and [PATCH_DAY.md](PATCH_DAY.md).

## Contents

1. [Words used here](#1-words-used-here)
2. [Threads: what may run where](#2-threads-what-may-run-where)
3. [Game work: Ask, step, publish](#3-game-work-ask-step-publish)
4. [Leaving the game: hooks.quit and hooks.stop](#4-leaving-the-game-hooksquit-and-hooksstop)
5. [Crash safety: the ledger](#5-crash-safety-the-ledger)
6. [Game builds: guards and self-disable](#6-game-builds-guards-and-self-disable)
7. [Game objects and game state](#7-game-objects-and-game-state)
8. [Shipping](#8-shipping)
9. [Language, toasts, tags and keys](#9-language-toasts-tags-and-keys)
10. [The generic test suites](#10-the-generic-test-suites)

## 1. Words used here

- **Present unit**: a source file under `src`, outside `src\overlay`, other than `src\entry.cpp`. It is compiled without ImGui.
- **Overlay unit**: a file under `src\overlay`. It may use ImGui and the `zsdk::tab` and `zsdk::paint` helpers.
- **The pump**: the SDK's tick on the game thread. The SDK hooks the game's message loop and runs one tick when its wake message arrives.
- **Game call**: a call of a game function through `zsdk::Call` and `zsdk::Fire`.
- **Key**: the name of a game call or of a block of your own code in the crash ledger, for example `Actor.K2_GetActorLocation` or `myhud.camera`.
- **Lane**: the height on the screen where your toasts are drawn, as a fraction of the screen height. The SDK's crash fence has lanes of another kind: 16 of them, one for each piece of work in the ReShade events (the add-on's hooks and the SDK's own work), each with the ledger key `present.<lane>` ([section 5.5](#55-when-something-faults)).

## 2. Threads: what may run where

An add-on runs on six kinds of thread. Every hook runs on one of them.

| thread | hooks that run there | may | may not |
|---|---|---|---|
| **present**: the render thread, ReShade's `reshade_present` | `boot` (once, at the first present), `frame(Input)`, `opened(bool)` | read `Input`; read settings (`ini::Config`); ask the game thread for work with `thread::Ask(order)`; push toasts; log; read copies (`world::Snapshot()`, your own snapshot under your own lock); register jobs with `job::Every` (only in `boot`) | ImGui, `zsdk::tab`, `zsdk::paint` (a present unit does not compile with them); build a `Call` or `Fire` (refused); write game memory (refused); `world::Hold()` (you get a copy and a log line); wait or block, because it holds the frame |
| **overlay**: the render thread inside ReShade's ImGui frame (`reshade_overlay` and the add-on's tab) | `overlay(Overlay&)`, `page(Overlay&)`, `words`, `tone`; `report` when the player presses "Copy report" (it only returns text) | ImGui, `zsdk::tab`, `zsdk::paint` (in overlay units); read your snapshot under your lock; change settings with `ini::Apply`; toasts | build a `Call` or `Fire`; write game memory; `world::Hold()`; slow work, because it runs every frame |
| **D3D**: ReShade's `init_pipeline`, `bind_pipeline`, `destroy_pipeline` and `destroy_device`, on any render, RHI or shader-compile thread, often several at once, and already before `boot` | `pipelineInit`, `pipelineBind`, `pipelineDestroy`, `deviceDestroy` | the ReShade device and command list; lock-free reads; short locks of your own | `Fire`, `world::Hold()`, ImGui, crash windows, ini writes; anything slow in `pipelineBind`, which runs for every bound pipeline: keep it O(1) |
| **game thread**: the pump | `refresh`, `prime`, `advance`, `step(now)`, `hot`, `publish(now)`, `turn`, `order`, `orderName`; sometimes `quit` | game calls; game memory writes inside a crash window (`mem::Write`); `world::Hold()`; toasts; settings | ImGui (pump code is present code); sleep or wait for another thread; long work: a tick over 5 ms is logged as `slow tick` |
| **SDK worker**: one thread per add-on, named `<add-on name> census` | `census(snapshot)`, `complete(snapshot)`, your jobs; sometimes `quit` | compute; read files; read game memory through the `ue::` and `mem::Read` helpers, which never fault; log; push toasts; hand a result to the game thread with `thread::Ask(order)` | build a `Call` or `Fire`, write game memory (both refused off the pump); ImGui; threads of your own; one run longer than 1 s |
| **loader**: `DllMain`, under the Windows loader lock | `stop` | set your own flags; drop in-memory state | locks, waits, file I/O, game calls, ImGui, new threads ([section 4](#4-leaving-the-game-hooksquit-and-hooksstop)) |

What follows from the table:

- Hooks are named functions, wired in `src/entry.cpp` with `h.<hook> = &Function;`, never lambdas. `overlay` and `page` are defined in overlay units, `words` and `tone` in either kind of unit, every other hook in a present unit: a file under `src` other than `src/entry.cpp` and `src/overlay`. `src/entry.cpp` itself only wires the hooks. The `zsdk-present` suite checks this.
- A present unit includes no ImGui and no `reshade.hpp`, and calls no function that only overlay units define. The compiler, the object symbol check of the build and the `zsdk-present` suite each catch a different way of breaking this.
- Off the game thread, read the world with `world::Snapshot()`. `world::Hold()` called from another thread logs `world: the grip was read outside the game thread, this thread gets its own copy from world::Snapshot()` and returns a copy that belongs to that thread and stays valid only until the next `Hold()` on it.
- The GPU hooks and `deviceDestroy` can run before `boot`. The SDK opens the data folder first, so the settings and the disabled keys are read before such a hook runs, but nothing that `boot` sets up exists yet.
- On a game build that ZoneSDK cannot bind, none of the pump hooks from `refresh` to `orderName` runs. The present, overlay and D3D hooks and the jobs still run.

## 3. Game work: Ask, step, publish

Game calls run only in the pump. Building a `Call` anywhere else throws `zsdk::Refused` ("a call was built off the game thread pump"), and so does `Fire` ("refused off-thread engine call"); no game code runs. So an add-on splits its work like this:

1. **Ask (present thread).** `hooks.frame` sees a key press and calls `zsdk::thread::Ask(order)`. An order is an `int` of your own. Ask queues it and wakes the game thread.
2. **Order (game thread).** The SDK calls `hooks.order(order)` early in the next tick, before `hooks.step`. At most 2 orders run per tick, the queue keeps the newest 8, and an order older than 2 s or from an earlier world is dropped with the log line `request <name> dropped (...)`, where `<name>` comes from `hooks.orderName`.
3. **Step (game thread).** `hooks.step(now)` makes the game calls and keeps the results in variables that only the game thread touches.
4. **Publish (game thread).** `hooks.publish(now)` copies what the overlay needs into a snapshot under your own mutex, every 250 ms, or every tick while `hooks.hot()` returns true.
5. **Draw (overlay).** `hooks.page` and `hooks.overlay` take the same mutex, copy the snapshot and draw from the copy.

While `hooks.hot()` returns true or an order waits, the SDK wakes the game thread on every present; otherwise about once a second. `hooks.step` normally runs at most once per game frame, and only once the engine is bound and wired.

The shape, as in the sample `ZoneHelloHud`:

```cpp
void Frame(const zsdk::Input &input)
{
    if (!input.Focused() || input.Overlaid() || input.Menu()) return;
    if (input.Fires(toggle)) zsdk::thread::Ask(static_cast<int>(Order::Toggle));
}

void OnOrder(int order)
{
    if (order == static_cast<int>(Order::Toggle)) live = !live;
}

void Step(uint64_t)
{
    const zsdk::world::Grip &w = zsdk::world::Hold();
    if (!live || !w.stable || !zsdk::ue::Here(w.body, w.bodyType) || !zsdk::verbs::Ready(kLocation)) return;
    try
    {
        zsdk::Call c(kSpecs, kLocation, w.body);
        zsdk::Fire(c);
        reading = c.Get<zsdk::math::Vec3>("ReturnValue");
    }
    catch (const std::exception &error)
    {
        zsdk::Rare("myhud read", "myhud: reading failed: %s", error.what());
    }
}

void Publish(uint64_t)
{
    std::lock_guard<std::mutex> hold(gate);
    shown = reading;
}

zsdk::math::Vec3 Shown()
{
    std::lock_guard<std::mutex> hold(gate);
    return shown;
}
```

### No threads of your own: job::Every

Background work runs as a job on the SDK worker, never on a thread, timer or thread-pool callback of your own. The `zsdk-hardrules` suite fails on `CreateThread`, `std::thread`, `std::jthread`, `std::async`, `_beginthread(ex)`, `CreateTimerQueueTimer`, `QueueUserWorkItem`, `TrySubmitThreadpoolCallback`, `SetTimer` and `CreateRemoteThread(Ex)`.

```cpp
void Scan(uint64_t now)
{
    if (zsdk::job::Stopping()) return;
    const int seen = Count(now);
    {
        std::lock_guard<std::mutex> hold(gate);
        found = seen;
    }
    zsdk::thread::Ask(static_cast<int>(Order::Found));
}

void Boot()
{
    zsdk::job::Every("myhud.scan", 500, &Scan);
}
```

- Register jobs in `hooks.boot` (`h.boot = &Boot;` in `src/entry.cpp`), before the SDK worker starts. A later call is refused with a log line. At most 8 jobs (`job::kMost`), a period of 100 ms or more, a name of 1 to 32 letters, digits, `.`, `_` or `-`.
- The period is the shortest gap between two runs. Jobs run on the worker's 100 ms tick, so a 100 ms job may in practice run only about every 200 ms.
- Jobs run one after another and on every game build, also when ZoneSDK cannot bind the engine. A job makes no game calls and writes no game memory (both are refused off the pump). It hands its result over through your own lock and `thread::Ask(order)`.
- A long job polls `job::Stopping()`. It turns true when the SDK stops, when the game asks to exit and at the process exit.
- A run over 50 ms is logged. A run over 1 s (`job::kStallMs`) switches that job off for the session. A C++ exception is logged (at most once a minute per job) and the job keeps its period. A structured exception (an access violation, a stack overflow) switches the job off for the session and disables the ledger key `job.<name>` until Try again ([section 5](#5-crash-safety-the-ledger)).
- Nothing can stop a job that never returns. After 1 s the SDK logs once `job <name> has not returned for <n> ms, the SDK worker waits for it: no other job, census step or exit poll runs until it returns`, so poll `job::Stopping()` and return.

## 4. Leaving the game: hooks.quit and hooks.stop

Two hooks run at the end, and they promise different things.

**`hooks.quit`: the game asked to exit.** The SDK watches the game's own exit flag (`GIsRequestingExit`) on the game builds it knows, and also reacts to `WM_QUIT`. The first of them starts one exit sequence: no new tick starts, `thread::Stopping()` and `job::Stopping()` turn true, every later `Fire` is refused with "engine calls are stopped", then `hooks.quit` runs once and the log gets `game exit requested (GIsRequestingExit)` or `game exit requested (WM_QUIT)`.

- It runs on the game thread or on the SDK worker, never at the same time as `hooks.step`.
- Game calls are already stopped: no `Fire`, no `Call`, no `mem::Write`.
- It may write your own files, take your own locks briefly and tell your own work to stop. It should not wait long: the game is closing.
- A job or census run that had already started may still be finishing on the worker.
- Do not rely on it. On a game build ZoneSDK does not know, only `WM_QUIT` is left, and this game rarely delivers it; a crash or a killed process never runs `hooks.quit`. Save what matters during the session. Settings changed through `ini::Apply` are already saved about a second after the last change.

**`hooks.stop`: the process exits or the add-on is unloaded.** It runs in `DllMain` under the Windows loader lock. At a process exit (`shell::Exiting()` is true) every other thread is already gone, maybe in the middle of holding a lock.

- No locks, no waits, no file I/O, no game calls, no ImGui, no new threads.
- Set your own flags and drop in-memory state, nothing more.
- `world::Snapshot()` and `census::Current()` only try their locks there and give an empty result when a dead thread held them.
- The only file the SDK itself writes at that point is its pre-built exit mark ([section 5.3](#53-the-exit-mark)).
- `hooks.stop` runs inside the SDK fence: a structured exception or a C++ exception there ends the hook, and the exit goes on without a log line, which the loader lock does not allow.

```cpp
void Stop()
{
    live = false;
}
```

## 5. Crash safety: the ledger

A wrong game call or a wrong memory address can crash the game. The ledger makes sure the same mistake cannot crash it twice: the next start finds what was running and switches exactly that off.

### 5.1 Crash markers

- `Fire` marks a game call the first time its key runs on a game build: it writes the key into the add-on's ini and flushes it before the call, and removes it after. A call that returns proves its key for this game build and this add-on build, so later calls of that key run without writing anything.
- If the game crashes while a key is marked, the next start disables that key and only that key. The log says `safety: <key> pending or watched when the last run ended, disabled`, Diagnostics shows "Disabled after a crash: <key>", and a later call of that key is refused with "disabled after a crash: <key>".
- A new game build or a new build of your add-on starts the proofs over, so first calls carry their markers again.
- When a marker cannot be written (no data folder, a locked ini), the call is refused with "crash marker unavailable: <key>" (`Policy::strictMarks`, on by default). Diagnostics then says the crash markers are not saved.
- The keys `engine.wire`, `census.hook`, `job.<name>` and `present.<lane>` belong to the SDK. Give your own keys a prefix of your add-on, for example `myhud.camera`.

### 5.2 Windows for your own risky code

Code that does not go through `Fire`, such as a raw memory read or write, gets its marker from a `ledger::Window`:

```cpp
zsdk::ledger::Window window("myhud.camera");
if (window.Refused()) return;
float fov = 0.0f;
const bool read = zsdk::mem::Read(zsdk::Addr(camera), kLockedFov, fov);
window.Close();
```

- `Close()` proves the key for this build. Leaving the scope without `Close()` removes the marker without a proof, so the next use marks again.
- A window is refused when its key is disabled after a crash or its marker could not be written. Skip the work then.
- `mem::Write` needs an open window that is not refused, a key that is not disabled, and the game thread inside the pump.
- At most 8 windows are open at a time (`Policy::watchKept`, 1 to 32).

### 5.3 The exit mark

At a normal process exit, after the last ReShade effect runtime is gone, the SDK writes a small mark file next to the ini: `<ini name>.exit`, for example `myhud.exit` for `myhud.ini`. It writes it with one pre-built write: no lock, no allocation, no log. After that the ini is frozen and nothing writes it again in that process.

The next start reads the mark:
- the mark fits the last run: `safety: the last run <session> ended cleanly (exit mark found)`. Windows that were left open are closed, not disabled; a game call that was still running counts as a crash;
- no mark: `safety: no clean exit mark from the last run <session>` (logged when a crash marker or a window was left set);
- a mark of another run, or an ini changed after the mark: `safety: the exit mark does not match the last run <session>, it counts as a crash`.

In both cases the run counts as a crash.

The mark works on every game build, so a normal quit is never read as a crash, even when `hooks.quit` did not run.

### 5.4 Try again

Diagnostics lists the keys that are disabled after a crash and shows the button "Спробувати ще / Try again" (`ledger::Retry()`). It clears the list; the next use of each key runs with its marker again. The log says `safety: try again, cleared <keys>`.

The fence keys `present.<lane>` ([section 5.5](#55-when-something-faults)) clear the same way. While the lane of the add-on's tab is off, the tab shows the SDK's own text instead, "Частини аддона <name> вимкнено після помилки: <parts>. Подробиці в журналі. / Parts of <name> were switched off after an error: <parts>. The log has the details.", where the parts are named in the player's language, followed by the log path, the same button and "Скопіювати звіт / Copy report". A lane runs again right after Try again (`safety: the <lanes> lanes run again after Try again`); after a fault in the boot or core lane in the same session, the add-on itself starts again only with the next game start.

### 5.5 When something faults

| where | C++ exception | structured exception (access violation, stack overflow) |
|---|---|---|
| a game call (`Call`, `Fire`) | a refusal or a bad parameter throws `zsdk::Refused` or `std::runtime_error` to you; nothing is disabled | caught inside `Fire`: the key is disabled until Try again and `zsdk::Fault` is thrown to you |
| your code in a pump hook | one log line (`<stage> error: ...` or `game thread error: ...`); the next tick runs | `structured exception 0x<code> at <module>+0x<offset> in the game thread tick`; with `Policy::haltOnSeh` (on by default) every game call is refused for the rest of the session |
| a job | logged, the job keeps running | the job is off for the session, `job.<name>` is disabled until Try again |
| `hooks.census`, `hooks.complete` | logged, the hook stays on | both hooks are off for the session, `census.hook` is disabled until Try again; the census goes on |
| `hooks.quit` | logged, the exit goes on | logged, the exit goes on |
| `hooks.stop` | the hook ends, nothing is logged, the exit goes on | the hook ends, nothing is logged, the exit goes on |
| a ReShade callback (`boot`, `frame`, `overlay`, `page`, `opened`, `deviceDestroy`, the GPU hooks, and the SDK's own work in the same events) | `<lane> error: ...` is logged at most once a minute per lane, for example `frame error: ...`; the callback runs again next time | the SDK fence switches that callback's lane off for the session, logs one line, `structured exception 0x<code> at <module>+0x<offset> in the <lane> lane, ...`, and writes the log to disk at once; the ledger key `present.<lane>` (for example `present.frame` or `present.pipelineBind`) stays disabled in later sessions until Try again. The frame and toasts lanes go off together. A fault while the add-on boots (where `hooks.boot` runs) or in the SDK's core switches the whole add-on off for the session, except its cleanup and save at the end. A fault in a GPU or device hook that came before the first present is kept too: the key is disabled once the data folder is open, with the log line `safety: the <lanes> lanes faulted before the data folder opened, they stay disabled`. The lanes are listed in [API.md](API.md#zsdkhooksh) |

The halt policy decides how far a fault inside `Fire` reaches:
- `Halt::Key` (the default): only the key that faulted is disabled. Every other call keeps working.
- `Halt::Session`: the faulting key is disabled and every game call is refused for the rest of the session. The phase shows Halted ("Game calls are stopped for this session"); after a halt the pump only publishes.

Changing `halt` or `haltOnSeh` from the default is listed as a legacy rule in the boot log and in Diagnostics.

## 6. Game builds: guards and self-disable

Every game update can move code and data. An add-on that reads a fixed address on a build it was not made for reads garbage, so every raw offset needs a proof, and a feature whose proof fails switches itself off instead of crashing.

- **Reflection first.** A function called through `Call`/`Fire` is checked against the game's live reflection once the engine is ready. A field the game describes in its reflection is a reflected `guard::Field`: the SDK compares the offset with `ue::PropertyOffset` before the first use. Both keep working after a game update as long as the game still describes them.
- **Guarded fields.** A field the game does not describe is a `guard::Field` proven by a `guard::Guard`: a short masked run of machine-code bytes at an RVA that contains the offset. The consteval constructor does not compile unless the offset sits in those bytes. `Field::Rip` proves an address the same way. A `guard::Set` groups guards and names the build they come from:
  - `Need::Bytes`: every guard's bytes match;
  - `Need::ImageSize`: and the exe image has the build's size;
  - `Need::KnownBuild`: and the exe has the build's exact SHA-256.
- **Unproven fields.** `Field::Unproven` compiles but is never usable. List each one with a reason in `tests/unproven-fields.txt` (`zsdk-offsets`).
- **Self-disable.** A field whose set is off is refused by `guard::Use`, `mem::Read` and `mem::Write`, with one log line `guard: field <name> is not usable on this build (...)`. Gate each feature with `guard::Of(set) == zsdk::guard::State::Ok`; `guard::Ready(set)` tells that the set has a verdict.
- **No raw offsets.** A constant such as `kHealthAt = 0x2C8`, a literal offset inside a memory access, or `+ 0x...` arithmetic fails `zsdk-offsets`. `field.Raw()` is the visible escape for code that gates by hand; the suite counts every use.

### App::sets: list every guard set

```cpp
inline constexpr zsdk::guard::Guard kCoreGuards[] = { { .rva = 0x22B5CDE, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00" } };
inline constexpr zsdk::guard::Set kCoreSet{ "myhud.core", &zsdk::guard::k2bd3f115, kCoreGuards, 1, zsdk::guard::Need::KnownBuild };
constexpr const zsdk::guard::Set *kSets[] = { &kCoreSet };
a.sets = kSets;
```

- The SDK checks every set in `App::sets` when the engine is ready, with one log line per set: `guard set myhud.core: 1/1 match, image 0xB954000, on`.
- The `zsdk-guards` suite compares only the listed sets with a game exe. A set that your sources declare but `App::sets` does not list is a warning, and a failure under `-RequireExe`.
- Give every set a literal name, as the first value in the braces as above or as a designated `.name = "..."`: the suite finds declared sets by that string.
- Names are unique, and `zsdk.*` names belong to the SDK's own sets.
- `build.cmd -Tests -Exe "<path to a Stalker2-Win64-Shipping.exe>"` compares every set with that exe, read-only. On the set's own build every guard must match; on any other build the suite prints an on/off forecast and does not fail. `-LiveExe` finds the installed game through Steam, and `-RequireExe` makes a missing exe or a set that comes out off a failure. A set pinned to another build than the exe (one set per game build, see [PATCH_DAY.md](PATCH_DAY.md)) is reported as `the set is for build <id> and the exe is build <id>` and is not required.

## 7. Game objects and game state

- **An object pointer is a guess.** The game can destroy any object at any time: streaming, death, a level change. Keep a `ue::Mark` from `ue::Seal(thing)` and check `ue::Ours(mark)` on the game thread right before every use. `ue::Doomed(thing)` is true for an object that is being destroyed, is garbage or is unreachable.
- Under `Policy::verifiedOnly` (on by default) a game call refuses a doomed target and a doomed object passed as a parameter.
- **Drop old pointers on a world turn.** `hooks.turn` runs when the world or the player's pawn changes; forget every pointer from before the turn there.
- **Keep objects through engine references.** To keep something alive, let the game own a reference to it: a property or a component of an object the game keeps. Never set the root bit or any other flag in the engine's object array by hand. This game's collector reads its own root list, not the bit, and a raw root bit even makes the game's own root call skip that object. ZoneSDK has no API for it, and `zsdk-hardrules` bans the raw write calls (`WriteProcessMemory`, `VirtualProtect`, `_InterlockedOr`).
- **Keep game changes reversible within the session.** What you change in the game can end up in the player's save. Record every change and undo it when the feature is turned off or the world turns. Nothing can be undone at exit: `hooks.quit` runs after game calls are stopped.
- **Leave dangerous calls alone.** The `banned=` list in `tests/zsdk.cfg` names calls an add-on must not make: saves, free camera, no-clip, faction relations, spawning or removing anomalies, physics and movement switches. The starter add-on ships a default list; keep it, and add to it.

## 8. Shipping

- **Never ship a Debug build.** `build.cmd -Config Debug` writes `bin\Debug\<Name>.addon64`, the Release build stays in `bin\<Name>.addon64`. `build.cmd -Verify` refuses a Debug build, and `pack.cmd` packs only the Release file and refuses a binary marked Debug.
- **Verify before you ship.** `.\build.cmd -Tests -Verify` builds, runs the tests (so `pack.cmd` knows the binary passed them), runs the add-on in the fake host, checks that it exports exactly `NAME` and `DESCRIPTION`, has a version resource and no Debug flag, and that the binary holds no build path, user profile path, path with your Windows user name in it or other absolute drive path.
- **Ship the notice.** `pack.cmd` puts `NOTICE-ADDON.txt` into the zip. If you pack by hand, include it yourself.
- **Ship no game files.** Never put a game exe or game assets into your add-on or its repository.

## 9. Language, toasts, tags and keys

### Language

- An add-on speaks Ukrainian and English only. Unless the player picks a language in its settings, it shows Ukrainian with the game in Ukrainian and English with every other game language, Russian included. There is no Russian option.
- The settings offer `auto`, `uk` and `en`, and a fresh ini holds `auto`. `lang::Detect()` reads the game's own text language (its settings file, then the Steam language of the game, then the Windows language); a stored `ru` or any unknown code becomes `auto`.
- Every text row has both a Ukrainian and an English text, the same `{n}` slots in both, no Russian letters or words, and only signs the SDK font can draw. The `zsdk-texts` suite checks all of it.
- The SDK ships default rows for Diagnostics, the key field and the phases (`lang::Defaults()`); a row of yours with the same id wins.

### Toasts

- `App::row.lane` is the height where your toasts start, as a fraction of the screen height. Pick one no other add-on uses: `new-addon` picks a free lane from the ZoneSDK table, and `zsdk-identity` warns when your lane is shared.
- Lane `0` means no toasts. With lane 0 the SDK's own warning toast for an unsupported game build is not drawn either; Diagnostics still shows it.
- An add-on with no row gets the spare lane 0.75 and one log line.
- `toast::Push(kind, ...)` takes kinds 0 and up; kinds below 0 belong to the SDK.

### Tags

- `Identity::tag` names the add-on's wake message (`<tag>.GameThread.1`) and its one-instance mutex (`Local\<tag>.Addon.<pid>`). Plain ASCII letters and digits only.
- It must be unique among all add-ons a player has. With two add-ons of one tag the second does not load: `DllMain` returns FALSE and sends one line to the debugger output (visible in DebugView), `ZoneSDK: <name> does not load: the tag <tag> is already taken in this process ...`.
- Put your author name in front, for example `-Tag AuthorMyHud` for `new-addon`. Give `Identity::folder` a unique name too: it is your folder under `%LOCALAPPDATA%`.

### Keys

- Default to an Alt chord, for example `ALT+H`: the game binds plain keys, rarely Alt chords. Never default to a numpad key: many keyboards have none, and `new-addon` refuses them.
- Let the player rebind every key with `tab::KeyField`. The key field notes "The game uses this key too" for a key the game binds on its own, unless the chord holds ALT; a CTRL or SHIFT chord with such a key still gets the note, so check it in game.
- Do not act on keys while the ReShade overlay or a game menu is open or the game window has no focus: check `input.Focused()`, `input.Overlaid()` and `input.Menu()` first.
- `new-addon` picks a key that no row in the ZoneSDK table, no sample and no add-on next to the target uses, and `zsdk-identity` warns when an add-on next to yours has the same default key. Another add-on a player has may still use it, so check it in game.

## 10. The generic test suites

`build.cmd -Tests` (or `test.cmd`) builds the add-on, then runs your own tests and the ZoneSDK generic suites. Each suite prints one line, `zsdk-<suite>: N checks, M failures`; a failed check prints `FAIL <message>` above it, and a failed generic suite adds the line `zsdk-<suite>: docs\RULES.md (docs\RULES-UA.md in Ukrainian), section 10, zsdk-<suite>, gives the rule behind each message of this suite` under it. A line that starts with `warning:` does not fail the build; the summary counts them as test warnings.

### zsdk.cfg

`tests\zsdk.cfg` holds one `key=value` per line. A list is comma-separated, and a path is relative to the add-on folder, written with forward slashes (`src/poke.cpp`). Keys the suites read:

| key | used by | meaning |
|---|---|---|
| `name` | zsdk-style | the add-on name; must equal the name the build uses |
| `comments` | zsdk-style | `forbid` turns on the no-comments check over `src`, `tests`, the `.rc` files and `build\make.ps1`; `allow` or no key leaves comments allowed. The starter add-on ships without it |
| `banned` | zsdk-style | call names that may not appear in `src`, as a word or inside a string |
| `facade` | zsdk-hardrules | files that may reach the raw ledger (`zsdk/test/ledger_raw.h`, `ledger::raw`), only inside `#if defined(ZSDK_TESTS)` blocks |
| `memory` | zsdk-hardrules | files that may write game memory with `mem::Poke`, without a crash window |
| `identity.same` | zsdk-identity | folder names of other add-on trees that may use your tag, such as an old copy of your own add-on |
| `workspace` | zsdk-identity | the folder the tag-collision scan looks through, relative to the add-on folder |
| `define` | zsdk-identity | the macro prefix of the add-on; must equal the define the build uses. `new-addon` writes it |
| `registry` | zsdk-identity | the registry id of the add-on; must equal `Identity::registry`. `new-addon` writes it |
| `present.allow` | zsdk-present | function names that present units may call although only overlay units define a function of that name |

Environment variables: `ZSDK_WORKSPACE` overrides `workspace=`. `ZSDK_EXE` and `ZSDK_REQUIRE_EXE=1` drive `zsdk-guards`; `build.cmd -Exe`, `-LiveExe` and `-RequireExe` set them for you.

### zsdk-present

Present units stay free of ImGui and overlay code; hooks are named functions in the right kind of unit. Rules: [section 2](#2-threads-what-may-run-where).

| message after FAIL | what to do |
|---|---|
| `the add-on tree has present units and src/entry.cpp` | keep `src/entry.cpp` and at least one present unit |
| `no <word> in the present unit <file>` | move ImGui, `reshade.hpp`, `zsdk/overlay.h`, `zsdk/paint.h`, `paint::`, `tab::`, `overlay::` or `IM_COL32` code into `src/overlay` |
| `the present unit <file> calls the overlay-only function <name>` | call it from an overlay unit; if it is another function of the same name, add the name to `present.allow` |
| `the build wrote the object symbol table: <path>`, `the present objects reference external symbols`, `the overlay objects define symbols that no present object defines` | the symbol check found nothing to check; build with the add-on's own `build.cmd`, which writes the table |
| `a present object uses a symbol defined only in overlay objects: <object> <symbol>`, `no present object references an overlay-only symbol (...)` | a present unit links to overlay code, maybe through a hand-written declaration; move the call to an overlay unit |
| `entry.cpp registers through zsdk::shell::Main`, `no <word> in entry.cpp` | `src/entry.cpp` only wires hooks and hands `DllMain` to `zsdk::shell::Main`; no ImGui there |
| `the hook <hook> is wired to a lambda; hooks are named functions`, `the hook <hook> is wired to a named function: <target>` | write `h.<hook> = &Function;` |
| `the hook <hook> is a known ZoneSDK hook` | the field is not a `zsdk::Hooks` member |
| `the hook <hook> is an overlay function defined in src/overlay: <name>`, with `, which is defined in src/entry.cpp`, `, which a present unit defines too` or `, which no overlay unit defines` | define `overlay` and `page` in `src/overlay`, once |
| `the hook <hook> is a function of a present unit (present units are src/*.cpp and their subfolders, except src/entry.cpp and src/overlay): <name>`, with the place where `<name>` was found | define the hook in a present unit: not in `src/entry.cpp`, which only wires hooks, and not in `src/overlay`; two functions of one name in a present and an overlay unit need different names |
| `the toast hook <hook> is one add-on function: <name>` | define `words` and `tone` once, in one unit |
| `entry.cpp wires the present hooks, <n>`, `entry.cpp wires the tab page` | wire at least two present hooks and `page` |
| `the overlay-only call set is not empty, <n>` | the ZoneSDK release is broken; report it |

### zsdk-hardrules

No native shortcuts, no threads of your own, no raw writes, a clean entry, and a build against the pinned release. Rules: sections [3](#3-game-work-ask-step-publish), [5](#5-crash-safety-the-ledger) and [7](#7-game-objects-and-game-state).

| message after FAIL | what to do |
|---|---|
| `no <word> in <file>` for `Ride(`, `ProcessEvent`, `__try`, `__except`, `native_bindings::`, `detail::engine`, `detail/`, `zsdk::detail` | call game functions only through `Call`/`Fire`; the SDK's internals and SEH fences are not for add-on code |
| `no <word> in <file>` for `SetWindowsHookEx`, `VirtualProtect`, `WriteProcessMemory`, `_InterlockedOr` | no hooks or raw writes of your own; write game memory with `mem::Write` inside a crash window |
| `no <word> in <file>: background work runs through zsdk::job::Every on the SDK worker, ...` for `CreateThread`, `std::thread` and the timer and thread-pool calls | use `job::Every` ([section 3](#no-threads-of-your-own-jobevery)) |
| `only the listed test facade reaches the raw ledger: <file>`, `the facade reaches the raw ledger only inside #if defined(ZSDK_TESTS) blocks: <file>` | the raw ledger is for tests; list the file in `facade=` and keep the use inside `#if defined(ZSDK_TESTS)` |
| `raw game memory writes outside a crash window only in a listed memory facade: <file>` | use `mem::Write` with a `ledger::Window`, or list the file in `memory=` |
| `exports come only from ZSDK_ADDON: <file>` | no `dllexport` of your own |
| `one ZSDK_ADDON and one DllMain`, `DllMain only hands over to zsdk::shell::Main` | keep the starter's `src/entry.cpp` shape |
| `zsdk.lock pins this ZoneSDK version`, `zsdk.lock pins the manifest of the release it builds against` | `build\zsdk.lock` names another release than the one the build found; fix `version=` and `manifest=` ([VERSIONING.md](VERSIONING.md)) |
| `the add-on builds against a release snapshot, not the working tree: <path>` | build against a `release\<version>` folder of the package |
| `tests drive no native code: <file>` | no `__try` and no `ProcessEvent` in `tests` |

Until a ZoneSDK release ships `zsdk/job.h`, the timer and thread-pool words are only warnings (`hardrules: <word> in <file>: ...`). ZoneSDK 1.0 ships it, so they fail.

### zsdk-style

The add-on's config, comments, literals and `build\make.ps1`. Rules: [section 10](#zsdkcfg) and [section 7](#7-game-objects-and-game-state).

| message after FAIL | what to do |
|---|---|
| `tests/zsdk.cfg exists`, `zsdk.cfg names the add-on that is built` | keep `tests\zsdk.cfg` with `name=<the add-on name>` |
| `zsdk.cfg comments= is forbid or allow: <value>` | use one of the two words |
| `no comments in <file> (line <n>)`, `no comments in make.ps1 line <n>` | only with `comments=forbid`: remove the comment |
| `every literal is closed in <file>` | a string or character literal is not closed |
| `build/make.ps1 exists`, `make.ps1 builds through the pinned ZoneSDK module` | keep the starter's `build\make.ps1` |
| `make.ps1 is pure ASCII, line <n>`, `make.ps1 has no absolute drive path` | no other letters and no drive paths in `build\make.ps1` |
| `build/make.ps1 deploys nothing: no <word>` | the build never copies into the game folder; installing is the player's step |
| `forbidden call <name> is absent from <file>` | the call is on the `banned=` list ([section 7](#7-game-objects-and-game-state)) |

Warning only: `style: a raw tick difference '<text>' in <file> line <n>, ...`. Write `zsdk::Age(now, at)`, which gives 0 when the stamp is later than now, instead of `now - at`.

### zsdk-texts

The language rule. Rules: [section 9](#language).

| message after FAIL | what to do |
|---|---|
| `the add-on has language rows, <n>`, `row ids are unique: <id>` | give every text an id of its own |
| `both languages have text: <id>` | fill in the Ukrainian and the English text |
| `no Russian letters: <id>` | the text has Russian letters or words; write Ukrainian |
| `uk and en use the same {n} slots: <id>` | both languages use the same `{0}`, `{1}` ... |
| `the paint font draws every sign of <id> <U+...>` | replace the listed signs |
| `no Russian language is offered in <file>: "<literal>"` | remove the Russian language code (`ru`, `rus`, `russian`, `ru-ru`, `ru_ru`, or an id ending in `.ru`, `.rus`, `.russian`) |
| `a fresh ini gives the language auto (the game language: uk gives UA, every other culture EN), the add-on default is <code>` | make `auto` the default of your language setting |
| `a fresh data folder opens` | the settings schema cannot open a fresh ini; check `Schema()` |
| `ru never selects a Russian option and equals Detect()`, `the SDK knows Ukrainian and English only`, `the SDK ships default rows, <n>`, `the SDK default row has both languages: <id>`, `no Russian in the SDK default row <id>`, `uk and en use the same {n} slots in the SDK default row <id>`, `the paint font draws the SDK default row <id>` | the ZoneSDK release is broken; report it |

`SKIPPED zsdk-texts: the add-on has no settings schema ...` comes only together with a `zsdk-identity` failure: ZoneSDK 1.0 needs a settings schema.

### zsdk-identity

Unique names. Rules: [section 9](#tags).

| message after FAIL | what to do |
|---|---|
| `the identity has a name, a tag and a version`, `the identity name is the built add-on` | fill in `App::identity`; its name is the add-on name |
| `the App has a settings schema (App::schema); without one ZoneSDK opens no data folder, settings or log for the add-on and leaves it off` | set `App::schema` to an `ini::Schema` (the starter has one in `src/settings.cpp`) |
| `zsdk.cfg registry= names the registry id of the identity: <cfg> against <id>`, `zsdk.cfg define= is the define the build uses: <cfg> against <define>` | make `registry=` and `define=` in `tests\zsdk.cfg` match `Identity::registry` and the build's define, or remove the lines |
| `the tag is plain ASCII letters and digits` | change the tag |
| `the add-on has a registry row, its own or the ZoneSDK one: <id>`, `the registry row carries the same tag`, `the registry row has the identity registry id` | set `App::row` with the same id and tag as `App::identity` |
| `the ZoneSDK table agrees with the add-on row for <id>`, `no other registry row uses the tag <tag>: <id>` | the id or tag is taken in the ZoneSDK table; pick another |
| `another tree uses the tag <tag> for its wake or mutex name: <path>` | another add-on folder next to yours uses your tag; change the tag, or list that folder in `identity.same` if it is your own copy |
| `the workspace for the collision scan exists: <path>` | `workspace=` or `ZSDK_WORKSPACE` points at a missing folder |
| `registry tags are unique: <tag>`, `the wake message derives from the tag`, `the mutex derives from the tag` | the ZoneSDK release is broken; report it |

Warnings only:
- `identity: <id> is not in the ZoneSDK registry table, its own row is used`: normal for every add-on the ZoneSDK table does not list.
- `identity: toast lane <lane> is shared with <id>`: pick another lane.
- `identity: toast lane <lane> is also used by the add-on in <folder>; its toasts and yours would overlap` and `identity: the key <key> is also a default key of the add-on in <folder>`: an add-on next to yours uses the same lane or default key; pick another one.
- `identity: the collision scan stopped after 5000 folders ...`: point `workspace=` or `ZSDK_WORKSPACE` at the folder that holds your add-ons.

By default the scan looks at the folders next to your add-on that carry `build\zsdk.lock`.

### zsdk-offsets

Raw offsets need a proof. Rules: [section 6](#6-game-builds-guards-and-self-disable).

| message after FAIL | what to do |
|---|---|
| `a raw layout constant outside zsdk::guard::Field: <name> in <file>` | make the offset a `guard::Field` with a guard, RIP or reflection proof |
| `a literal offset inside a memory access: <text> in <file>` | read and write through a `guard::Field` |
| `hex literal arithmetic, an offset belongs in a Field: <text> in <file>` | no `+ 0x...` on addresses; use a `guard::Field` |
| `the unproven field <name> is listed with a reason in tests/unproven-fields.txt` | add `<name>: <reason>` to `tests\unproven-fields.txt` |
| `the listed unproven field <name> still exists` | remove the line of a field that no longer exists or is proven now |

### zsdk-specs

Your verb table against the game's function dump. Rules: [section 3](#3-game-work-ask-step-publish).

| message after FAIL | what to do |
|---|---|
| `the add-on has a verb table` | `App::specs` is empty |
| `WorldVerbs gives <Owner.Name> the index of that verb in App::specs, or -1 for none (...)` | `WorldVerbs{}` means the catalog indices of `verbs::common::kWorld`. Start your table with it (`verbs::Join(common::kWorld, kOwn)`), or set every `WorldVerbs` role yourself, `-1` for none. ZoneSDK 0.1.x read `WorldVerbs{}` as "no world verbs", so an add-on moved from 0.1.x sets the roles itself |
| `the dump of <build> agrees with <Owner.Name> (<why>)` | the function's parameters, offsets, sizes or block size differ from your `Spec`; fix the `Spec` |
| `the function dump of <build> is readable, <n> rows` | the release's `data\builds\<build>\ufunctions.txt` is broken; report it |

`SKIPPED zsdk-specs: the ZoneSDK release has no function dump ...` means the release carries no `ufunctions.txt`; the verbs are then checked only in game. A verb that is not in the dump is only counted. The SDK checks every verb against the game's live reflection once the engine is ready, and a role that names another verb is switched off at the first world step with one log line.

### zsdk-guards

Guard sets against a real game exe. Rules: [section 6](#6-game-builds-guards-and-self-disable).

| message after FAIL | what to do |
|---|---|
| `an add-on guard set does not take the zsdk.* names of the SDK's own sets` | rename the set |
| `every guard set has a name`, `guard set names are unique: <name>` | give every set its own name |
| `the guard set <name> points at its guards` | the set has a count but no guards |
| `the guard set <name> names the build its image size comes from` | a set with `Need::ImageSize` or `Need::KnownBuild` names its `guard::Build` |
| `the guard bytes parse: <name> <rva>` | write bytes as two-digit hex pairs separated by spaces, `??` for a masked byte |
| `the guard lies inside the image of <build>: <name> <rva>` | the RVA plus the bytes runs past the image |
| `the exe is build <id> of the set <name> by SHA-256, so every guard and the image size match, ...` | on the set's own build a guard differs: the bytes or the RVA are wrong |
| `the guard set <name> (...) is not listed in App.sets ...; under -RequireExe every declared set is listed in .sets` | list the set in `App::sets` |
| `the guard set <id> in <file> has no literal name, ...; under -RequireExe ...` | write the name as a string literal, as the first value in the braces or as `.name = "..."` |
| `an exe is given for the guard check under -RequireExe (...)`, `the exe <file> opens read-only as a PE image under -RequireExe (...)` | pass `-Exe` with a readable exe, or `-LiveExe` |
| `the set <name> comes out on under -RequireExe, ...` | under `-RequireExe` every set must be on for the given exe, except a set pinned to another build |
| `the SDK's own guard sets are named zsdk.*` | the ZoneSDK release is broken; report it |

Without an exe the suite prints `SKIPPED zsdk-guards: no exe given (ZSDK_EXE, make -Exe or -LiveExe), ...` and opens no file; an exe it cannot read gives `SKIPPED zsdk-guards: the exe <file> cannot be used (...)`. It never copies the exe.
