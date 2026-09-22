[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BuildDirectory,[ValidateRange(5,60)][int]$Seconds=20,[ValidateSet('PrivetDr.unr','Entry.unr','Entryhall_hub.unr')][string]$Map='PrivetDr.unr',[ValidateSet('observe','shift-x8','pair-control','pair-capture')][string]$CameraMode='observe')
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
if(@(Get-Process Game -ErrorAction SilentlyContinue | Where-Object {-not $_.HasExited}).Count){throw 'Game.exe laeuft bereits. Keine parallele Instrumentierung.'}
$config=Get-ProjectConfig
$build=Resolve-ProjectPath $BuildDirectory
$record=Get-Content -LiteralPath (Resolve-ProjectPath (Join-Path $build 'build.json')) -Raw|ConvertFrom-Json
$exe=Resolve-ProjectPath $record.executable
if((Get-FileHash -LiteralPath $exe).Hash -ne $record.executable_sha256){throw 'Observer-Binaerdatei veraendert'}
foreach($r in (Get-Content -LiteralPath (Resolve-ProjectPath 'config\target-fingerprint.json') -Raw|ConvertFrom-Json).files){if((Get-FileHash -LiteralPath (Resolve-ProjectPath $r.path)).Hash -ne $r.sha256){throw "Originalversion abweichend: $($r.path)"}}
& python -B (Resolve-ProjectPath 'scripts\backup_original.py') --reason 'Before bounded native engine observer'
if($LASTEXITCODE -ne 0){throw 'Backup fehlgeschlagen'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$profile=Resolve-ProjectPath "cache\profile-$stamp"
$stageArgs=@('-B',(Resolve-ProjectPath 'scripts\stage_test_profile.py'),'--output',$profile)
& python @stageArgs
if($LASTEXITCODE -ne 0){throw 'Testprofil fehlgeschlagen'}
$before=@(Get-SafeFiles $config.profile_directory|ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$stdout=Resolve-ProjectPath "logs\engine-observer-$stamp.jsonl"
$stderr=Resolve-ProjectPath "logs\engine-observer-$stamp.stderr.txt"
Write-Host 'Begrenzter Test einer Originalszene mit Hardware-Debugpunkten; kein VR-Start.'
Write-Host 'Profilumleitung wird vor Spielinitialisierung geprueft. Windows/Treiber koennen trotzdem externe Daten schreiben.'
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
    $env:TEMP=New-ProjectDirectory 'cache\tmp';$env:TMP=$env:TEMP
    & $exe (Resolve-ProjectPath $config.game_executable) $profile $Seconds $Map $CameraMode 1> $stdout 2> $stderr
    $code=$LASTEXITCODE
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
$changed=@($before|Where-Object {(Get-FileHash -LiteralPath $_.path).Hash -ne $_.sha256}|ForEach-Object {$_.path})
$added=@(Get-SafeFiles $config.profile_directory|Where-Object {$_.FullName -notin $before.path}|ForEach-Object {$_.FullName})
$run=[ordered]@{date_utc=[DateTime]::UtcNow.ToString('o');observer_exit_code=$code;build=$BuildDirectory;map=$Map;seconds=$Seconds;camera_mode=$CameraMode;profile=$profile;trace=$stdout;stderr=$stderr;external_profile_changed=$changed;external_profile_added=$added;headset_tested=$false}
Write-NewProjectText "logs\engine-observer-$stamp.run.json" ($run|ConvertTo-Json -Depth 6)|Write-Output
Get-Content -LiteralPath $stdout -Tail 2
if($changed.Count -or $added.Count){throw 'Externes Profil hat sich veraendert; Bericht pruefen.'}
if($code -ne 0){Get-Content -LiteralPath $stderr;throw "Observer-Test nicht bestanden: Exit $code"}
