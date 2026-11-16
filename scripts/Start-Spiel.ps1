[CmdletBinding()]
param([string]$BuildDirectory,[switch]$Diagnostic)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
. (Join-Path $PSScriptRoot 'Mod.Locale.ps1')
$launchLockPath=Resolve-ProjectPath (Join-Path (New-ProjectDirectory 'cache/locks') 'menu-session.lock')
try {$launchLock=[System.IO.File]::Open($launchLockPath,[System.IO.FileMode]::OpenOrCreate,[System.IO.FileAccess]::ReadWrite,[System.IO.FileShare]::None)}
catch {throw (Get-ModText 'Der Spielstarter oder eine Spielsitzung ist bereits aktiv. Bitte nur ein Startfenster verwenden.' 'The launcher or a game session is already running. Use one launcher window.')}
try {
if(@(Get-Process Game -ErrorAction SilentlyContinue).Count){throw (Get-ModText 'Game.exe laeuft bereits. Bitte die laufende Sitzung erst beenden.' 'Game.exe is already running. Close the current session first.')}
if(-not $BuildDirectory){$BuildDirectory=(Get-Content -LiteralPath (Resolve-ProjectPath 'config/menu-current-build.json') -Raw|ConvertFrom-Json).build}
$settings=Resolve-ProjectPath 'config/launcher.ini'
if(-not (Test-Path -LiteralPath $settings)){Write-NewProjectText $settings "[Launch]`r`nMode=Flat`r`n" | Out-Null}
$build=Resolve-ProjectPath $BuildDirectory
$record=Get-Content -LiteralPath (Resolve-ProjectPath (Join-Path $build 'build.json')) -Raw|ConvertFrom-Json
$config=Get-ProjectConfig
Write-Host (Get-ModText 'Harry Potter II - Originales Hauptmenue mit Flat/VR-Schalter' 'Harry Potter II - Original main menu with Flat/VR switch')
Write-Host (Get-ModText 'Flat: unveraendertes Originalspiel mit seinen normalen Einstellungen und Spielstaenden unter Dokumente/Harry Potter II.' 'Flat: original game, using its normal settings and saves in Documents/Harry Potter II.')
Write-Host (Get-ModText 'VR: bisherige VR-Fassung. Flat und VR verwenden dieselben Speicherplaetze und denselben Fortschritt.' 'VR: VR mod. Flat and VR share save slots and progress.')
Write-Host (Get-ModText 'Vor VR bitte Quest ueber Steam Link verbinden. Grafik und Steuerung bleiben passend zum Modus.' 'Before VR, connect your headset to SteamVR. Graphics and controls follow the selected mode.')
Write-Host (Get-ModText 'Beim Umschalten wird das passende Hauptmenue neu geoeffnet. Dieses Startfenster bitte offen lassen.' 'Switching modes reopens the appropriate main menu. Keep this launcher window open.')
Write-Host (Get-ModText 'Gemeinsame Spielstaende: Dokumente/Harry Potter II/Save. Der Starter sichert und gleicht VR-Fortschritt dort ab.' 'Shared saves: Documents/Harry Potter II/Save. The launcher backs up and synchronizes VR progress there.')
Write-Host (Get-ModText 'Flat schreibt seine normalen Einstellungen. Windows und SteamVR koennen eigene Protokolle schreiben.' 'Flat writes its normal settings. Windows and SteamVR may write their own logs.')
while($true){
 $choices=@(Get-Content -LiteralPath $settings | Where-Object {$_ -match '^\s*Mode\s*=\s*(Flat|VR)\s*$'})
 if($choices.Count -ne 1){throw (Get-ModText 'Ungueltige Moduswahl in config/launcher.ini' 'Invalid mode in config/launcher.ini')}
 $mode=($choices[0] -split '=',2)[1].Trim();$vr=$mode -eq 'VR'
 $binaries=if($vr){@('dll','launcher')}else{@('flat_launcher')}
 foreach($name in $binaries){if((Get-FileHash -LiteralPath (Resolve-ProjectPath $record.$name)).Hash -ne $record.($name+'_sha256')){throw "Changed launcher component: $name"}}
 & python -B (Resolve-ProjectPath 'scripts/prepare_native_targets.py') --check
 if($LASTEXITCODE -ne 0){throw 'Original fingerprint mismatch'}
 & python -B (Resolve-ProjectPath 'scripts/backup_original.py') --reason "Before $mode original menu session"
 if($LASTEXITCODE -ne 0){throw 'Backup failed'}
 if(-not $Diagnostic){
  & python -B (Resolve-ProjectPath 'scripts/shared_saves.py') --recover
  if($LASTEXITCODE -ne 0){throw (Get-ModText 'Gemeinsame Spielstaende konnten nicht sicher abgeglichen werden. Alle Fassungen bleiben gesichert.' 'Shared saves could not be synchronized safely. All versions remain backed up.')}
 }
 $stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
 $stdout=Resolve-ProjectPath "logs/menu-$stamp.jsonl";$stderr=Resolve-ProjectPath "logs/menu-$stamp.stderr.txt"
 $profile=$config.profile_directory
 if($vr){
  $profile=Resolve-ProjectPath "cache/menu-profile-$stamp"
  $sharedArgs=@();if(-not $Diagnostic -or (Test-Path -LiteralPath (Resolve-ProjectPath 'config/shared-saves.json'))){$sharedArgs=@('--shared')}
  & python -B (Resolve-ProjectPath 'scripts/menu_sessions.py') --prepare $profile @sharedArgs
  if($LASTEXITCODE -ne 0){throw 'Menu profile preparation failed'}
  & python -B (Resolve-ProjectPath 'scripts/stage_vr_bindings.py') --profile $profile
  if($LASTEXITCODE -ne 0){throw 'VR controls staging failed'}
  if(-not $Diagnostic){
   & python -B (Resolve-ProjectPath 'scripts/menu_sessions.py') --activate $profile --trace $stdout
   if($LASTEXITCODE -ne 0){throw 'Session could not be retained before launch'}
   & python -B (Resolve-ProjectPath 'scripts/shared_saves.py') --begin $profile
   if($LASTEXITCODE -ne 0){throw (Get-ModText 'Gemeinsame Speicherstaende stimmen vor VR-Start nicht ueberein' 'Shared saves do not match before VR launch')}
  }
 }
 $before=@(Get-SafeFiles $config.profile_directory | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 $oldTemp=$env:TEMP;$oldTmp=$env:TMP
 try{
  $env:TEMP=New-ProjectDirectory 'cache/tmp';$env:TMP=$env:TEMP
  if($vr){
   & $record.launcher (Resolve-ProjectPath $config.game_executable) $profile 0 frontend native-menu 1> $stdout 2> $stderr
  }else{
   # No staging, VR arguments, debugger or injection on the original Flat path.
   & $record.flat_launcher (Resolve-ProjectPath $config.game_executable) 1> $stdout 2> $stderr
  }
  $code=$LASTEXITCODE
 }finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
 $changed=@($before|Where-Object {(-not (Test-Path -LiteralPath $_.path)) -or (Get-FileHash -LiteralPath $_.path).Hash -ne $_.sha256}|ForEach-Object path)
 $added=@(Get-SafeFiles $config.profile_directory|Where-Object {$_.FullName -notin $before.path}|ForEach-Object FullName)
 Write-NewProjectText "logs/menu-$stamp.run.json" ([ordered]@{exit_code=$code;mode=$mode;profile=$profile;trace=$stdout;build=$BuildDirectory;diagnostic=[bool]$Diagnostic;external_profile_changed=$changed;external_profile_added=$added}|ConvertTo-Json -Depth 4)|Write-Output
 if($vr -and ($changed.Count -or $added.Count)){throw 'External original profile changed during VR session'}
 if($vr -and -not $Diagnostic){
  # Retain completed original book saves even when the game reports a crash.
  & python -B (Resolve-ProjectPath 'scripts/shared_saves.py') --finish $profile
  if($LASTEXITCODE -ne 0){throw (Get-ModText 'VR-Spielstaende sind gesichert, konnten aber nicht gemeinsam veroeffentlicht werden' 'VR saves are backed up but could not be synchronized with the shared profile')}
 }
 if($code -eq 42){
  $requests=@(Get-Content -LiteralPath $stdout | Where-Object {$_ -match '"event":"frontend_switch_requested"'} | ForEach-Object {$_|ConvertFrom-Json})
  if($requests.Count -ne 1 -or [bool]$requests[0].vr -eq $vr){throw 'Game exited with code 42 without a valid mode switch'}
  Write-Host (Get-ModText 'Modus wird gewechselt ...' 'Switching mode ...');continue
 }
 if($code -ne 0){Get-Content -LiteralPath $stderr;Write-Host (Get-ModText 'Die Sitzung wurde unterbrochen. Fertig geschriebene Spielstaende bleiben erhalten.' 'The session was interrupted. Completed saves remain available.');throw "Game session failed: $code"}
 if($vr -and -not $Diagnostic){
  & python -B (Resolve-ProjectPath 'scripts/menu_sessions.py') --publish $profile --trace $stdout
  if($LASTEXITCODE -ne 0){throw 'Session profile retained but continuation pointer could not be updated'}
 }
 if($Diagnostic){Write-Host 'Diagnostic session ended without publishing VR progress.'}
 break
}
} finally {$launchLock.Dispose()}
