HARRY POTTER II VR - v1.0.2

Install your own PC copy of Harry Potter and the Chamber of Secrets. Make sure
the vanilla game runs first. Extract this ZIP into its installation directory:
Start-HP2VR.cmd must be NEXT TO the existing System folder. No original files
are replaced. Run Start-HP2VR.cmd, then select Flat or VR in the original menu.
Start SteamVR and connect the headset before choosing VR. Existing desktop/CD
shortcuts still run vanilla; you may create a shortcut to Start-HP2VR.cmd.

Flat runs the original Game.exe without injection or VR modifications. VR uses
the mod. Both share the original six save slots. Changing mode reopens the main
menu. Leave the launcher open until the game exits and saves are synchronized.
Completed book saves are recovered on the next launch after a crash. Unfinished
Save.tmp files are never published. Do not run another game instance concurrently.

Requirements / compatibility
Windows 10/11 on an x64 PC, a working original installation, SteamVR and a
configured headset with two controllers. Engine build HPCos_021009_1205-1 is
supported: the launcher verifies Game.exe, engine components and HGame.u before
VR injection. Other releases/patches show a clear VR error and retain Flat mode.
Game language does not guarantee binary compatibility. No Linux/macOS support.

Direct3D 11 uses SteamVR's reported GPU, with no hard-coded GPU vendor. Other
PCs and headsets have not been physically tested. Quest/Touch has been tested;
see CONTROLLERS.txt for other mappings. Use SteamVR controller bindings for
driver-specific changes. A complete playthrough on all systems is not verified.
Wireless audio gaps and compression artifacts may still occur.

Language / performance
New mod UI follows Language in Game.ini, falling back to System/Default.ini.
German and English are included. Other game languages use English for new mod
text; the original game's text and voice language remain unchanged.
Edit HP2VR/Options.ini: Language=auto/de/en and Quality=performance/balanced/sharp.
These use 1280/1536/2048 square pixels per eye; balanced is the default. Try
performance if movement stutters. Restart the game to apply changes. No global
SteamVR, network or audio settings are changed. No startup internet download.
AutoQuality additionally caps resolution to 1280 below 2 GB local GPU memory budget
(or when memory is unknown), and 1536 below 4 GB. Set AutoQuality=0 in the user
data config/hp2vr.ini to disable this cap. Memory is not a frame-rate guarantee.

Touch controls (right hand)
Left stick: move/strafe. Right stick: turn. A: jump/flying action. Right trigger:
cast/throw/select. B: menu/back. X: confirm. Y: potion. Left trigger: map.
Hold left grip: hand status. Right grip: walk/broom boost. Left stick click:
recenter. Right stick click: extra save. Original save books still work.
The in-game controller bindings panel remaps actions. The menu also offers
cinema screen or 3D/360-degree cutscenes using the original scene camera.

Reporting problems
Run Diagnose-HP2VR.cmd beside the launcher. It creates a ZIP with launcher and
native logs, Windows version, GPU names and file checks. The report folder then
opens. Send the ZIP, a short error description and your headset model to the mod
maintainer. Personal paths and user/PC names are removed from report text.
Saved games and personal settings are not included or modified. Nothing is
uploaded automatically. There is no need to reproduce a crash. See
README-DIAGNOSE.txt for custom data paths and details. Error messages point to
this tool; startup failures are logged. v1.0.2 adds fallback paths when the
graphics driver rejects extra menu/HUD targets. The reported memory error was
simulated in tests; confirmation on the affected test PC is still pending.

Future updates
New versions will be published on the project's GitHub Releases page.
Close the game and follow the update instructions for that release.

Data / removal
Settings, logs, private runtime image and backups are stored in
%LOCALAPPDATA%/HP2VR/<installation-id>. No mod writes into System and no admin
rights are normally needed, including a read-only installation folder.
Shared saves remain in the Windows Documents/Harry Potter II/Save folder.
Cross-drive exchanges also retain backups under Save/.hp2vr-transactions.
Existing saves are never silently discarded. To share the mod, share ONLY the
original release ZIP, not your personal runtime, logs, backups or Save folders.

To uninstall, close the game and remove HP2VR, Start-HP2VR.cmd,
Diagnose-HP2VR.cmd and README-HP2VR.txt from the game
folder. Original game files and shared saves remain intact. Keep user data and
backups until you no longer need them. --check validates without launching;
--data-root PATH selects a shorter data path for long Windows user paths.

The package contains mod components and bundled runtime licenses only: no game
EXE, game assets, music, saved games or personal profiles. A separate original
game installation is required. Unofficial fan mod, unaffiliated with the game
rights holders or SteamVR.
