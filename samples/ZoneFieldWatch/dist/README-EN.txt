ZoneFieldWatch {{VERSION}}
A ReShade add-on for S.T.A.L.K.E.R. 2: Heart of Chornobyl, a ZoneSDK sample.
It reads two fields of the player camera and shows them in its tab. It changes nothing in the game.
Author: {{AUTHOR}}

REQUIREMENTS
- S.T.A.L.K.E.R. 2: Heart of Chornobyl, Steam version, one of these game builds: {{GAME}}.
  On another game build the add-on still starts: the field checked against the game reflection can keep working,
  the byte-guarded field stays off until an update.
- ReShade 6.8 or newer, the "with full add-on support" download, installed for the game (DirectX 12).

INSTALL
1. Close the game.
2. Unpack this archive into the game folder, the one that holds the Stalker2 folder.
   ZoneFieldWatch.addon64 must end up in Stalker2\Binaries\Win64, next to dxgi.dll of ReShade.
   A mod manager can install the archive as it is.
3. Start the game, press Home to open ReShade and open the ZoneFieldWatch tab in the ReShade window.

WHAT THE TAB SHOWS
- Camera.DefaultFOV: the default field of view, checked against the game reflection.
- Camera.LockedFOV: the locked field of view, proven by the bytes of the game code, on game builds 2bd3f115 and 61bc1e03 only.
- Camera.ViewFOV: a field the add-on never reads, because it is not proven.
- The game build, the state of the guard set for it, and the crash markers: a read that was running when the game crashed
  is switched off, and Try again in Diagnostics switches it on again.

FILES
Settings and the log live in %LOCALAPPDATA%\ZoneFieldWatch (fieldwatch.ini, fieldwatch.log).

REMOVE
Delete Stalker2\Binaries\Win64\ZoneFieldWatch.addon64.
To remove the settings and the log too, delete the folder %LOCALAPPDATA%\ZoneFieldWatch.

BUG REPORTS
Write on the Nexus Mods page of the add-on. Attach %LOCALAPPDATA%\ZoneFieldWatch\fieldwatch.log
and ReShade.log from Stalker2\Binaries\Win64, and name your game build.

Built with ZoneSDK {{ZSDK}}. The licences of the parts inside are in NOTICE-ADDON.txt.
