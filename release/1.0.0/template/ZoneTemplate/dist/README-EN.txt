ZoneTemplate {{VERSION}}
A ReShade add-on for S.T.A.L.K.E.R. 2: Heart of Chornobyl.
Author: {{AUTHOR}}

REQUIREMENTS
- S.T.A.L.K.E.R. 2: Heart of Chornobyl, Steam version, one of these game builds: {{GAME}}.
  On another game build the add-on still starts, but its game features stay off until an update.
- ReShade 6.8 or newer, the "with full add-on support" download, installed for the game (DirectX 12).

INSTALL
1. Close the game.
2. Unpack this archive into the game folder, the one that holds the Stalker2 folder.
   ZoneTemplate.addon64 must end up in Stalker2\Binaries\Win64, next to dxgi.dll of ReShade.
   A mod manager can install the archive as it is.
3. Start the game, press Home to open ReShade and open the ZoneTemplate tab in the ReShade window.

KEYS
- ALT+L: the main action of the add-on. You can change the key in the ZoneTemplate tab.

FILES
Settings and the log live in %LOCALAPPDATA%\ZoneTemplate (template.ini, template.log).

REMOVE
Delete Stalker2\Binaries\Win64\ZoneTemplate.addon64.
To remove the settings and the log too, delete the folder %LOCALAPPDATA%\ZoneTemplate.

BUG REPORTS
Write on the Nexus Mods page of the add-on. Attach %LOCALAPPDATA%\ZoneTemplate\template.log
and ReShade.log from Stalker2\Binaries\Win64, and name your game build.

Built with ZoneSDK {{ZSDK}}. The licences of the parts inside are in NOTICE-ADDON.txt.
