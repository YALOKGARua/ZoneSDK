# ZoneHelloHud, ZoneSDK sample 1

A small add-on that draws a panel in a screen corner with the player's position (X, Y, Z in metres), compass heading and flat speed. It is meant to be read: every file shows one part of the ZoneSDK way of writing an add-on. It ships as sources only.

## What it shows, file by file

| File | What to look at |
| --- | --- |
| `src/hellohud.h` | The verb table: `verbs::Join(common::kWorld, kOwn)` puts the shared world catalog first, so `WorldVerbs{}` needs no indices. The own verbs are designated `verbs::Spec` rows for three reflected `Actor` functions: `K2_GetActorLocation` (required), `GetVelocity` and `K2_GetActorRotation` (optional; the Blueprint names of the last two are GetVelocity and GetActorRotation). The settings, the order and the published `Status`. |
| `src/app.cpp` | The `App` built with designated initializers: identity, default policy, census statics, registry row with tag, toast lane and default key. |
| `src/settings.cpp` | The ini schema: `language` (auto, uk, en; auto follows the game), flags, `ini::Choice` for the corner and the speed unit, and a key written by name. |
| `src/frame.cpp` | The frame hook. It never calls the game: on the key it calls `thread::Ask(Order::Toggle)`, and the SDK runs the order on the game thread. |
| `src/core.cpp` | The game thread: `Order` flips the panel and saves it, `Step` reads the three values every 100 ms through `Call` and `Fire` (each call carries its crash marker on first use), `Hot` keeps the pump awake only while the panel is shown in a stable world, `Publish` copies the state under a mutex and `Snapshot` hands a copy to the drawing side. `Section` adds a line to the Copy report text. |
| `src/view.cpp` | Pure functions with no game and no ImGui: the bearing (north is -Y), the compass point, the speed, the lines in Ukrainian or English, the corner placement. `tests/local.cpp` tests them. |
| `src/overlay/corner.cpp` | The overlay hook: `paint::Box`, `paint::Corners` and `paint::Text` on the foreground draw list, the font that draws Cyrillic, the palette from `paint::ink()`. |
| `src/overlay/page.cpp` | The ReShade tab: language, flags, corner and unit pickers, `tab::KeyField` for the key, the live lines, toasts through `Words` and `Tone`, and `tab::Diagnostics(o)` with the SDK default texts. |
| `src/lang_rows.cpp` | UA and EN rows; the SDK default rows fill the Diagnostics and key texts. |
| `tests/local.cpp` | Settings round trip, the key, the registry row, the verb table, the compass, the lines, the corners, the toggle order, the saved ini, and publish against snapshot from a second thread. |

No raw game memory is read: the values come only from reflected functions, and the add-on has no `guard::Field`. The code has no comments, and `tests/zsdk.cfg` turns the no-comments check on with `comments=forbid`.

## Build and check

From the unpacked ZoneSDK folder, in `samples\ZoneHelloHud`:

1. `.\test.cmd`: a Release build and the tests. `build\zsdk.lock` pins the ZoneSDK release of this package, and `build\make.ps1` finds `release\<version>` by itself.
2. `.\fakehost.cmd -Addon samples\ZoneHelloHud\bin\ZoneHelloHud.addon64 -Label hellohud -All` from the ZoneSDK folder: the offline runs without the game.
3. `.\build.cmd -Tests -Verify`: the tests again and the checks for a finished file. A build without `-Tests` makes `pack.cmd` warn that the binary was not built with passing tests.
4. `.\pack.cmd`: writes `dist\ZoneHelloHud-<version>.zip` with the player readmes from `dist\`.

In game the panel shows only in the game world, after a save is loaded; ALT+7 shows or hides it, and the tab works everywhere.

## Your own add-on

Start a new add-on with `new-addon.cmd`, which writes a fresh tag, key, lane and lock, and take the parts you need from here. Do not ship this sample with its tag ZoneHelloHud: two add-ons with one tag never load together, and `new-addon.cmd` refuses the name and the tag of a sample.
