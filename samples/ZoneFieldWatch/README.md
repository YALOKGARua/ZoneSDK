# ZoneFieldWatch

A ZoneSDK sample add-on that reads game memory the safe way. It watches two fields of the player camera manager (`PlayerCameraManager`) and shows them in its ReShade tab:

| field | offset | proof | what happens on a game build this ZoneSDK does not know |
|---|---|---|---|
| `Camera.DefaultFOV` | +0x2C8 | **reflected**: checked against the game's own reflection | keeps working while the reflection agrees |
| `Camera.LockedFOV` | +0x2CC | **byte guard**: one guard set with `Need::KnownBuild` per known game build | switches itself off |
| `Camera.ViewFOV` | +0x17F0 | **unproven**: listed in `tests/unproven-fields.txt` | never read on any build |

The sample makes no game calls of its own and changes nothing in the game. The code has no comments (the ZoneSDK house rule, `comments=forbid` in `tests/zsdk.cfg`), so this file explains it.

## Files

- `src/fields.h`: the guard sets, one per known game build, and the fields. Start here.
- `src/watch.cpp`: the guarded field of the running build (`On`, `Locked`), the verdict of a field (`Gate`), the read inside a crash window (`Read`), the tab view of a field (`Live`) and the build verdict (`Look`).
- `src/core.cpp`: the hooks. `prime` runs the reflection check, `step` reads both fields on the game thread every 250 ms, `publish` hands the readings to the tab under a mutex, and the job `fieldwatch.verdict` logs the build verdict.
- `src/overlay/page.cpp`: the tab: the watched fields, the block "Game build and verdicts" and the SDK Diagnostics.
- `tests/watch.cpp`: the offline proof of the proofs, the refusals on an unknown build and the crash markers.
- `tests/unproven-fields.txt`: the list of unproven fields with their reasons.

## 1. A reflected field

```cpp
inline constexpr guard::Field kDefaultFov{ "Camera.DefaultFOV", 0x2C8, "PlayerCameraManager", "DefaultFOV" };
```

`DefaultFOV` is a reflected property, so the game itself knows its offset. The `prime` hook (game thread, engine ready) calls `guard::Confirm(kDefaultFov)`: the SDK finds the class `PlayerCameraManager`, asks for the offset of `DefaultFOV` and compares it with `0x2C8`. Until that check has passed, `guard::Usable` is false and `mem::Read` refuses the field. On a mismatch the SDK writes one line, `guard: field Camera.DefaultFOV is +0x... in PlayerCameraManager.DefaultFOV, expected +0x2C8, its features are off`, and the field stays off for the session. A reflected field keeps working after a game patch as long as the reflection agrees, so prefer it whenever the game describes the field.

## 2. A byte-guarded field with `Need::KnownBuild`

```cpp
inline constexpr guard::Guard kCameraGuards61bc1e03[] = {
    { .rva = 0x23CA0E4, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 ... 48 8D 15 ?? ?? ?? ?? ..." },
};
inline constexpr guard::Guard kCameraGuards2bd3f115[] = {
    { .rva = 0x22B5CDE, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 ... 48 8D 15 ?? ?? ?? ?? ..." },
};
inline constexpr guard::Set kCamera61bc1e03{ "fieldwatch.camera.61bc1e03", &guard::k61bc1e03, kCameraGuards61bc1e03, 1, guard::Need::KnownBuild };
inline constexpr guard::Set kCamera2bd3f115{ "fieldwatch.camera.2bd3f115", &guard::k2bd3f115, kCameraGuards2bd3f115, 1, guard::Need::KnownBuild };
inline constexpr guard::Field kLockedFov61bc1e03{ "Camera.LockedFOV", 0x2CC, kCamera61bc1e03, 0, 8 };
inline constexpr guard::Field kLockedFov2bd3f115{ "Camera.LockedFOV", 0x2CC, kCamera2bd3f115, 0, 8 };
```

`LockedFOV` is a native member without reflection. Its proof is machine code: `APlayerCameraManager::GetFOVAngle` starts with `sub rsp, 28h` followed by `movss xmm0, [rcx+2CCh]`. The game build 61bc1e03 (game version 2.0.6) has it at RVA `0x23CA0E4`, and the build 2bd3f115 (game version 2.1) at RVA `0x22B5CDE`, with the same bytes. The bytes `CC 02 00 00` stand at byte 8 of each guard, and the consteval `Field` constructor refuses to compile unless `0x2CC` is exactly there. The four bytes of the `lea` displacement are masked with `??`; the rest of the function stays in the guard.

- `Need::KnownBuild` turns a set on only when the guard bytes, the image size and the exact SHA-256 of the exe all match its build, `k61bc1e03` or `k2bd3f115`, so on any exe at most one of the two sets is on. Until the SDK has hashed the exe both sets are `Pending`, and the tab says "waiting for its check". `Need::Bytes` and `Need::ImageSize` are the weaker levels.
- Both sets are listed in `App.sets` (`a.sets = fields::kSets;`). Listed sets are checked at prime, with one log line each, and they are the only sets the generic `guards` suite compares with an exe. A set that is declared but not listed gives a test warning, and with `-RequireExe` a failure.
- `watch::Locked()` gives the field of `fields::kLockedFovs` whose set is on; while no set is on, the first field stands in. On a game build that neither set names, both sets are off and every read through it is refused with the line `guard: field Camera.LockedFOV is not usable on this build (guard set is not on), its read or write is refused`.

Use this level for fields the game does not describe, and keep one guard set per game build. The set for 2bd3f115 was added when game version 2.1 came out: the same bytes were found again in the new exe, and the set and its field were added beside the old ones.

## 3. An unproven field and `tests/unproven-fields.txt`

```cpp
inline constexpr guard::Field kViewFov = guard::Field::Unproven("Camera.ViewFOV", 0x17F0);
```

The FOV of the cached camera view sits at +0x17F0, but `GetFOVAngle` reaches it in two steps (`add rcx, 17C0h` and then `movss xmm0, [rcx+30h]`), so no single guard holds the whole offset, and it is not a reflected property of its own. `Field::Unproven` compiles, but such a field is never usable: `guard::Usable` is false and `mem::Read` refuses it on every build.

The generic `offsets` suite keeps this debt visible:

- every `Field::Unproven("<name>", ...)` in `src` must have a line `<name>: <reason>` in `tests/unproven-fields.txt`, with a reason after the first colon;
- every such line must still name an unproven field that the sources declare;
- the suite reads every line with a colon as such an entry, so keep one line per field and nothing else in the file.

When you find a proof later, turn the field into a guard or reflected field and delete its line, or the suite fails.

## 4. Reads and crash markers

`watch::Read(object, field, key)` reads one field in this order:

1. If `ledger::Blocked(key)`, the read was disabled after a crash: verdict "disabled after a crash", nothing is read.
2. `Gate(field)`: a field that is unproven, off or still waiting for its check is not read, and no crash window is opened for it.
3. No key, no read: a read without a crash marker key is refused.
4. No object (no stable world or no player camera): "waiting for the player camera".
5. `ledger::Window window(key)` sets the crash marker. If it cannot be written, the read is skipped.
6. `mem::Read(object, field, value)` reads through the field. It checks the field again and copies through a fault-safe copy.
7. `window.Close()` clears the marker and proves the key for this exe.

What the marker does:

- The first read of a key on an exe writes `watch=<key>` into the add-on ini before the read and removes it after. `Close()` stores the key under `[proven]` with the short hash of the exe, so later reads on the same exe write nothing.
- If the game dies while a marker is set, the next start disables that key (`disabled=<key>`) and only that key. The log says `fieldwatch: <key> was disabled at this boot, the game ended while its crash marker was set`, the tab shows "disabled after a crash", and "Try again" in Diagnostics (`ledger::Retry()`) enables it again.
- A new game build, or a new build of the add-on, starts the proofs again.

`tests/watch.cpp` proves this offline with a test field over the image header of the test exe: the marker is written once, the proof is kept, a window left open across a restart disables its key only, and `Retry` brings the read back.

## 5. Threads

- The reads run in `hooks.step`, on the game thread inside the SDK pump, at most every 250 ms.
- `hooks.publish` copies the readings into a snapshot under a mutex; the tab only reads that snapshot.
- The build verdict (known or unknown build, guard set on, off or pending, the verdict of each field) needs no world. The job `fieldwatch.verdict` (`job::Every`, registered in `hooks.boot`) works it out on the SDK worker every 500 ms and logs it when it changes. So the tab and the log give their verdict even when the game is not supported and the engine never binds: then ZoneSDK does not hash the exe either, the build shows as not supported, and every field that needs the engine or the hash is off.

## 6. The tab

- "Watched fields": each field with its value or its verdict, its proof and the state of its crash marker.
- "Game build and verdicts": the game build (known to this ZoneSDK, not known, not supported, or not checked yet) and the state of the guard set of the running build; while no set is on, it is shown as `fieldwatch.camera`.
- The SDK Diagnostics: phase, hook and pump, exe, functions, keys disabled after a crash with "Try again", the log path.

## 7. Build and test

- `.\test.cmd` builds Release and runs the suites: `local`, `watch` and the generic `zsdk-*` suites.
- `.\build.cmd -Tests -Exe "<game folder>\Stalker2\Binaries\Win64\Stalker2-Win64-Shipping.exe"` makes the `guards` suite compare `fieldwatch.camera.61bc1e03` and `fieldwatch.camera.2bd3f115` with that exe; it only reads the file. On build 2bd3f115 the set `fieldwatch.camera.2bd3f115` comes out on and the other one off, as a set for another build; on build 61bc1e03 it is the other way round; on any other build the suite prints a forecast and does not fail. Add `-RequireExe` to fail when a set comes out off, except a set pinned to another build; `-LiveExe` finds the installed game through Steam.
- `..\..\fakehost.cmd -Addon bin\ZoneFieldWatch.addon64 -Label fieldwatch` (the `fakehost.cmd` of the ZoneSDK folder) runs the add-on in the ZoneSDK fake host. The fake host has no engine, so ZoneSDK binds nothing and the add-on stays on its unsupported-build path with every feature off: its log says `fieldwatch: the game build is not supported, ZoneSDK did not bind the engine; guard set fieldwatch.camera off; Camera.DefaultFOV off, Camera.LockedFOV off, Camera.ViewFOV unproven, never read`. The other unknown-build path, an exe that binds but has another hash, is proven by `tests/watch.cpp`: there the log says `fieldwatch: game build <hash> is not known to this ZoneSDK; guard set fieldwatch.camera off; ...`.
- `.\build.cmd -Tests -Verify` runs the tests again and checks the finished file (fake host, exports, version resource, byte scan) before you ship it. After a build without `-Tests`, `pack.cmd` warns that the binary was not built with passing tests.

`build\zsdk.lock` pins the ZoneSDK release this sample is built against. In the ZoneSDK package the build finds it in `release\<version>` next to the `samples` folder.

## 8. On a game patch

If ZoneSDK still binds the engine on the new exe, it hashes the exe and finds a build it does not know: the reflected field keeps working if the reflection agrees, the byte-guarded field and its set switch off, and the tab says "Game build ... is not known to this ZoneSDK". If ZoneSDK cannot bind the engine, the build is not supported and nothing is read. Either way nothing crashes. To bring the byte-guarded field back, find the same code in the new exe, add a guard set and a field for the new build beside the old ones, list the set in `App.sets` and the field in `kLockedFovs`, and prove it with the `guards` suite and `-Exe`. Point the set at the build row of a ZoneSDK release that knows the build, or at a `guard::Build` of your own until one does. This sample did exactly that for game version 2.1: the 54 guard bytes moved from `0x23CA0E4` to `0x22B5CDE`, and ZoneSDK 1.0.0 knows the build as `guard::k2bd3f115`. The ZoneSDK page `PATCH_DAY.md` (section 4) tells the whole case.
