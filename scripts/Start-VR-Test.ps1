[CmdletBinding()]
param([ValidateRange(25,60)][int]$Seconds=45)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$selected=Get-Content -LiteralPath (Resolve-ProjectPath 'config\native-current-build.json') -Raw|ConvertFrom-Json
Write-Host 'HP2 VR: experimenteller Headset-Test in der Eingangshalle. Noch keine vollstaendige Spiel-Mod.'
Write-Host 'Quest mit Steam Link verbinden; SteamVR muss Headset und Controller als bereit anzeigen.'
& (Resolve-ProjectPath 'scripts\Test-VRRuntime.ps1') -BuildDirectory $selected.build
$resolution=if($selected.PSObject.Properties.Name -contains 'render_resolution'){$selected.render_resolution}else{'1280x960'}
& (Resolve-ProjectPath 'scripts\Test-NativeVR.ps1') -BuildDirectory $selected.build -Mode native-vr -Seconds $Seconds -Resolution $resolution
