[CmdletBinding()]
param([switch]$FreshSession)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$selected=Get-Content -LiteralPath (Resolve-ProjectPath 'config\native-current-build.json') -Raw|ConvertFrom-Json
Write-Host 'Harry Potter II VR - Fortsetzung vom letzten VR-Kontrollpunkt; sonst Eingangshalle'
Write-Host 'Keine automatische Zeitbegrenzung. Beenden: Stop-VR.cmd oder Alt+F4 im Spielfenster.'
Write-Host 'Dieses Fenster waehrend des Spiels offen lassen.'
Write-Host 'Quest ueber Steam Link verbinden; SteamVR muss bereit sein.'
Write-Host 'Links: laufen/seitwaerts; rechts: drehen; A: springen; B: Menue; rechter Trigger: zaubern/auswaehlen.'
Write-Host 'Sichtbarer Stab und VR-Buchmenue enthalten. Story-Fortschritt/HUD/Sondermechaniken noch nicht vollstaendig geprueft.'
& (Resolve-ProjectPath 'scripts\Test-VRRuntime.ps1') -BuildDirectory $selected.build
& (Resolve-ProjectPath 'scripts\Test-NativeVR.ps1') -BuildDirectory $selected.build -Mode native-vr -ManualExit -Resolution $selected.render_resolution -PlaySession -FreshSession:$FreshSession
