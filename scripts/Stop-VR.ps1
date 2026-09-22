[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$expected=Resolve-ProjectPath (Get-ProjectConfig).game_executable
$closed=0
foreach($gameProcess in @(Get-Process Game -ErrorAction SilentlyContinue)){
    $buildRoot=(Resolve-ProjectPath 'build')+'\'
    $privatePath=$gameProcess.Path
    if(-not $privatePath){continue}
    if(-not [string]::Equals($privatePath,$expected,[StringComparison]::OrdinalIgnoreCase) -and -not $privatePath.StartsWith($buildRoot,[StringComparison]::OrdinalIgnoreCase)){continue}
    $privatePath=Resolve-ProjectPath $privatePath
    $native=@($gameProcess.Modules|Where-Object {$_.ModuleName -eq 'hp2vr-native.dll'})
    if($native.Count -ne 1){continue}
    $modulePath=Resolve-ProjectPath $native[0].FileName
    $buildRoot=(Resolve-ProjectPath 'build')+'\'
    if(-not $modulePath.StartsWith($buildRoot,[StringComparison]::OrdinalIgnoreCase)){continue}
    if(-not $gameProcess.CloseMainWindow()){throw 'Kein schliessbares VR-Spielfenster gefunden. Alt+F4 im Spielfenster verwenden.'}
    if(-not $gameProcess.WaitForExit(15000)){throw 'Spiel hat sich noch nicht beendet; es wurde nicht gewaltsam geschlossen.'}
    $closed++
}
if($closed){Write-Host 'VR-Spiel regulaer beendet.'}else{Write-Host 'Keine laufende VR-Spielinstanz dieses Projekts gefunden.'}
