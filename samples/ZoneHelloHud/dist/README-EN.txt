ZoneHelloHud {{VERSION}}
A ReShade add-on for S.T.A.L.K.E.R. 2: Heart of Chornobyl, the first sample of ZoneSDK.
Author: {{AUTHOR}}

WHAT IT DOES
A small panel in a corner of the screen shows where you stand (X, Y and Z in metres),
which way you face (a compass bearing) and how fast you move. The ZoneHelloHud tab picks
the corner, km/h or m/s and the key, and shows the same lines and a Diagnostics block.

REQUIREMENTS
- S.T.A.L.K.E.R. 2: Heart of Chornobyl, Steam version, one of these game builds: {{GAME}}.
  On another game build the add-on still starts, but its game features stay off until an update.
- ReShade 6.8 or newer, the "with full add-on support" download, installed for the game (DirectX 12).

INSTALL
1. Close the game.
2. Unpack this archive into the game folder, the one that holds the Stalker2 folder.
   ZoneHelloHud.addon64 must end up in Stalker2\Binaries\Win64, next to dxgi.dll of ReShade.
   A mod manager can install the archive as it is.
3. Start the game, press Home to open ReShade and open the ZoneHelloHud tab in the ReShade window.

KEYS
- ALT+7: show or hide the panel while you are in the game world. You can change the key in the ZoneHelloHud tab.

FILES
Settings and the log live in %LOCALAPPDATA%\ZoneHelloHud (hellohud.ini, hellohud.log).

REMOVE
Delete Stalker2\Binaries\Win64\ZoneHelloHud.addon64.
To remove the settings and the log too, delete the folder %LOCALAPPDATA%\ZoneHelloHud.

BUG REPORTS
Write on the ZoneSDK page on Nexus Mods. Attach %LOCALAPPDATA%\ZoneHelloHud\hellohud.log
and ReShade.log from Stalker2\Binaries\Win64, and name your game build.

Built with ZoneSDK {{ZSDK}}. The licences of the parts inside are in NOTICE-ADDON.txt.
