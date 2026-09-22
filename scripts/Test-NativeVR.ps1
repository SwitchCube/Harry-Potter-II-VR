[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BuildDirectory,[ValidateSet('native-control','native-capture','native-vr','native-replay','native-input-replay')][string]$Mode='native-control',[ValidateRange(20,180)][int]$Seconds=25,[ValidateSet('auto','800x600','1280x960','1280x1280','1536x1536','2048x2048')][string]$Resolution='auto',[switch]$ManualExit,[ValidateSet('','texture','borrowed')][string]$HudFallback='',[ValidateSet('','carry','broom','reward','cinema','polish','mirror','vendor','stairs','sword')][string]$MechanicsReplay='',[switch]$TravelReplay,[switch]$SaveReplay,[switch]$ResizeReplay,[switch]$BookReplay,[switch]$HealthReplay,[switch]$ClimbReplay,[switch]$ClimbCourseReplay,[switch]$ClimbJumpReplay,[switch]$ClimbNoJumpReplay,[switch]$ClimbBlockReplay,[switch]$ClimbBlockPushReplay,[switch]$ClimbBlockStandingReplay,[switch]$StockFacingReplay,[switch]$BindingReplay,[switch]$StatusReplay,[switch]$LessonReplay,[switch]$AudioStallReplay,[switch]$StockAudioReplay,[switch]$FullHudBenchmark,[switch]$LookAroundReplay,[switch]$WindowReplay,[switch]$PlaySession,[switch]$FreshSession,[string]$SessionState='config/vr-session.json',[ValidateSet('Entryhall_hub.unr','Adv1Willow.unr','PrivetDr.unr','Grounds_Night.unr','Grounds_Day.unr','Grandstaircase_hub.unr','Ch1Rictusempra.unr','Quidditch_Intro.unr','Quidditch.unr','Ch3Diffindo.unr','Startup.unr')][string]$Map='Entryhall_hub.unr')
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
if(@(Get-Process Game -ErrorAction SilentlyContinue|Where-Object {-not $_.HasExited}).Count){throw 'Game.exe laeuft bereits'}
if($ManualExit -and $Mode -notin @('native-vr','native-replay')){throw 'ManualExit supports native-vr or the diagnostic native-replay only'}
if($TravelReplay -and ($Mode -ne 'native-input-replay' -or $Seconds -lt 45)){throw 'TravelReplay requires native-input-replay with at least 45 seconds'}
if(($SaveReplay -or $ResizeReplay) -and ($Mode -ne 'native-input-replay' -or $Seconds -lt 50)){throw 'Save/resize replay requires native-input-replay with at least 50 seconds'}
if($PlaySession -and $SessionState -eq 'config/vr-session.json' -and (-not $ManualExit -or $Mode -ne 'native-vr')){throw 'Production session persistence requires manual native-vr'}
if($BookReplay -and ($Mode -ne 'native-input-replay' -or $Seconds -lt 45)){throw 'BookReplay requires native-input-replay with at least 45 seconds'}
if($HealthReplay -and ($Mode -ne 'native-input-replay' -or $Seconds -lt 45)){throw 'HealthReplay requires native-input-replay with at least 45 seconds'}
$launcherMode=if($ManualExit){if($Mode -eq 'native-vr'){'native-play'}else{'native-play-replay'}}else{$Mode}
$launcherSeconds=if($ManualExit){0}else{$Seconds}
$build=Resolve-ProjectPath $BuildDirectory
$record=Get-Content -LiteralPath (Resolve-ProjectPath (Join-Path $build 'build.json')) -Raw|ConvertFrom-Json
foreach($name in @('dll','launcher')){if((Get-FileHash -LiteralPath (Resolve-ProjectPath $record.$name)).Hash -ne $record.($name+'_sha256')){throw "Changed native $name"}}
$sourceLock=Get-Content -LiteralPath (Resolve-ProjectPath 'config\research-sources.lock.json') -Raw|ConvertFrom-Json
$vrDll=@($sourceLock.files|Where-Object local_path -EQ 'external/openvr/bin/win32/openvr_api.dll')[0]
if(-not $vrDll -or (Get-FileHash -LiteralPath (Resolve-ProjectPath $vrDll.local_path)).Hash -ne $vrDll.sha256){throw 'OpenVR DLL fingerprint differs'}
& python -B (Resolve-ProjectPath 'scripts\prepare_native_targets.py') --check
if($LASTEXITCODE -ne 0){throw 'Original or native target mismatch'}
& python -B (Resolve-ProjectPath 'scripts\backup_original.py') --reason 'Before in-process native integration test'
if($LASTEXITCODE -ne 0){throw 'Backup failed'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$profile=Resolve-ProjectPath "cache\native-profile-$stamp"
$renderResolution=if($Resolution -ne 'auto'){$Resolution}elseif($Mode -eq 'native-vr'){'1280x960'}else{'800x600'}
$sessionArgs=@();if($PlaySession){$sessionArgs=@('--session-state',$SessionState);if($FreshSession){$sessionArgs+='--fresh-session'}}
& python -B (Resolve-ProjectPath 'scripts\stage_test_profile.py') --output $profile --resolution $renderResolution @sessionArgs
if($LASTEXITCODE -ne 0){throw 'Profile staging failed'}
if($HudFallback){if($Mode -notin @('native-replay','native-input-replay')){throw 'HUD failure injection is diagnostic only'};Write-NewProjectText (Join-Path $profile ('replay-hud-'+$HudFallback+'.flag')) 'Simulated DirectDraw video-memory failure'|Out-Null}
if($MechanicsReplay){if($Mode -ne 'native-input-replay'){throw 'Mechanics fixture requires hardware-free input replay'};Write-NewProjectText (Join-Path $profile 'replay-mechanics.flag') 'Isolated original game mechanics'|Out-Null;Write-NewProjectText (Join-Path $profile ('replay-mechanics-'+$MechanicsReplay+'.flag')) $MechanicsReplay|Out-Null}
if($MechanicsReplay -eq 'cinema' -or $BindingReplay){New-ProjectDirectory (Join-Path $profile 'config')|Out-Null}
if($WindowReplay){if($Mode -ne 'native-replay' -or $Seconds -lt 50){throw 'Window fixture requires at least 50 seconds of hardware-free replay'};Write-NewProjectText (Join-Path $profile 'replay-window.flag') 'Resize and maximize desktop mirror while preserving VR render size'|Out-Null}
if($LookAroundReplay){if($Mode -ne 'native-replay'){throw 'Look-around fixture requires hardware-free replay'};Write-NewProjectText (Join-Path $profile 'replay-look-around.flag') '360 degree original room visibility replay'|Out-Null}
if($AudioStallReplay){if(-not $LessonReplay){throw 'Audio stall test requires isolated lesson replay'};Write-NewProjectText (Join-Path $profile 'replay-audio-stall.flag') 'One 2500ms game thread stall during an original spoken sentence'|Out-Null}
if($StockAudioReplay){if(-not $AudioStallReplay){throw 'Stock audio requires audio stall replay'};Write-NewProjectText (Join-Path $profile 'replay-stock-audio.flag') 'Original 32768-byte PCM buffers for comparison'|Out-Null}
if($LessonReplay){if($Mode -ne 'native-input-replay' -or $Map -ne 'Grandstaircase_hub.unr'){throw 'Lesson replay requires original Grandstaircase input replay'};Write-NewProjectText (Join-Path $profile 'replay-lesson.flag') 'Original Rictusempra lesson; both sticks and original key reset logic'|Out-Null}
if($ClimbReplay){if($Mode -ne 'native-input-replay' -or $Map -ne 'Adv1Willow.unr'){throw 'ClimbReplay requires Willow input replay'};Write-NewProjectText (Join-Path $profile 'replay-climb.flag') 'Original Willow wall automatic mount without jump'|Out-Null}
if($ClimbCourseReplay){if(-not $ClimbReplay -or $ClimbJumpReplay){throw 'Course requires ClimbReplay and no jump variant'};Write-NewProjectText (Join-Path $profile 'replay-climb-course.flag') 'Two consecutive low ledges, continuous forward'|Out-Null}
if($ClimbJumpReplay){if(-not $ClimbReplay){throw 'Jump case requires ClimbReplay'};Write-NewProjectText (Join-Path $profile 'replay-climb-jump.flag') 'Original Willow high ledge, one jump'|Out-Null}
if($ClimbNoJumpReplay){if(-not $ClimbJumpReplay){throw 'No-jump baseline requires jump ledge fixture'};Write-NewProjectText (Join-Path $profile 'replay-climb-no-jump.flag') 'Same high ledge without jump, original limit retained'|Out-Null}
if($ClimbBlockReplay){if(-not $PlaySession -or $TravelReplay -or $SaveReplay -or -not $ClimbReplay -or -not $ClimbJumpReplay -or $ClimbCourseReplay -or $ClimbNoJumpReplay){throw 'Block requires a private checkpoint, climb and jump, with no travel/save or other ledge variants'};Write-NewProjectText (Join-Path $profile 'replay-climb-block.flag') 'Actual Willow GridMover1 original climb'|Out-Null}
if($ClimbBlockPushReplay){if(-not $ClimbBlockReplay){throw 'Push requires block fixture'};Write-NewProjectText (Join-Path $profile 'replay-climb-block-push.flag') 'Cast Flipendo before approaching actual block'|Out-Null}
if($ClimbBlockStandingReplay){if(-not $ClimbBlockReplay){throw 'Standing jump requires block fixture'};Write-NewProjectText (Join-Path $profile 'replay-climb-block-standing.flag') 'Walk against block for one second before jumping'|Out-Null}
if($StockFacingReplay){if(-not $ClimbReplay){throw 'StockFacingReplay requires ClimbReplay'};Write-NewProjectText (Join-Path $profile 'replay-stock-facing.flag') 'Diagnostic baseline without VR body-facing correction'|Out-Null}
if($BookReplay){Write-NewProjectText (Join-Path $profile 'replay-book.flag') 'Original book Touch handler in isolated diagnostic profile' | Out-Null}
if($FullHudBenchmark){Write-NewProjectText (Join-Path $profile 'replay-full-hud.flag') 'Original full-size HUD transfer for A/B performance measurement' | Out-Null}
if($BindingReplay){Write-NewProjectText (Join-Path $profile 'replay-binding-panel.flag') 'Diagnostic VR control panel' | Out-Null}
if($StatusReplay){Write-NewProjectText (Join-Path $profile 'replay-status.flag') 'Diagnostic status panel' | Out-Null}
if($HealthReplay){Write-NewProjectText (Join-Path $profile 'replay-health.flag') 'Render-only six-container health fixture; no saved health modifications' | Out-Null}
if($TravelReplay){Write-NewProjectText (Join-Path $profile 'replay-travel.flag') 'Two map transitions in isolated diagnostic profile' | Out-Null}
if($SaveReplay){Write-NewProjectText (Join-Path $profile 'replay-save.flag') 'Original save/load in isolated diagnostic profile' | Out-Null}
if($ResizeReplay){Write-NewProjectText (Join-Path $profile 'replay-resize.flag') 'Resize real D3D7 viewport twice' | Out-Null}
if($Mode -in @('native-vr','native-input-replay')){
    & python -B (Resolve-ProjectPath 'scripts\stage_vr_bindings.py') --profile $profile
    if($LASTEXITCODE -ne 0){throw 'VR bindings staging failed'}
}
$config=Get-ProjectConfig
$before=@(Get-SafeFiles $config.profile_directory | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$stdout=Resolve-ProjectPath "logs\native-$stamp.jsonl";$stderr=Resolve-ProjectPath "logs\native-$stamp.stderr.txt"
Write-Host 'Echte native DLL im eigenen Original-Spielprozess; Testprofil gesichert. Windows/Treiber koennen externe Daten schreiben.'
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
    $env:TEMP=New-ProjectDirectory 'cache\tmp';$env:TMP=$env:TEMP
    & $record.launcher (Resolve-ProjectPath $config.game_executable) $profile $launcherSeconds $Map $launcherMode 1> $stdout 2> $stderr
    $code=$LASTEXITCODE
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
$changed=@($before|Where-Object {(Get-FileHash -LiteralPath $_.path).Hash -ne $_.sha256}|ForEach-Object path)
$added=@(Get-SafeFiles $config.profile_directory|Where-Object {$_.FullName -notin $before.path}|ForEach-Object FullName)
$result=[ordered]@{date_utc=[DateTime]::UtcNow.ToString('o');exit_code=$code;build=$BuildDirectory;profile=$profile;trace=$stdout;stderr=$stderr;mode=$Mode;hud_fallback=$HudFallback;map=$Map;manual_exit=[bool]$ManualExit;travel_replay=[bool]$TravelReplay;save_replay=[bool]$SaveReplay;resize_replay=[bool]$ResizeReplay;mechanics_replay=$MechanicsReplay;window_replay=[bool]$WindowReplay;look_around_replay=[bool]$LookAroundReplay;lesson_replay=[bool]$LessonReplay;audio_stall_replay=[bool]$AudioStallReplay;stock_audio_replay=[bool]$StockAudioReplay;climb_replay=[bool]$ClimbReplay;climb_course_replay=[bool]$ClimbCourseReplay;climb_jump_replay=[bool]$ClimbJumpReplay;climb_no_jump_replay=[bool]$ClimbNoJumpReplay;climb_block_replay=[bool]$ClimbBlockReplay;climb_block_push_replay=[bool]$ClimbBlockPushReplay;climb_block_standing_replay=[bool]$ClimbBlockStandingReplay;stock_facing_replay=[bool]$StockFacingReplay;binding_replay=[bool]$BindingReplay;status_replay=[bool]$StatusReplay;book_replay=[bool]$BookReplay;health_replay=[bool]$HealthReplay;play_session=[bool]$PlaySession;resume_requested=(Test-Path -LiteralPath (Join-Path $profile 'resume-save.flag'));launcher_mode=$launcherMode;render_resolution=$renderResolution;external_profile_changed=$changed;external_profile_added=$added;headset_tested=$false;settings_sha256=(Get-FileHash -LiteralPath (Resolve-ProjectPath 'config\hp2vr.ini')).Hash}
Write-NewProjectText "logs\native-$stamp.run.json" ($result|ConvertTo-Json -Depth 5)|Write-Output
Get-Content -LiteralPath $stdout -Tail 5
if($changed.Count -or $added.Count){throw 'External profile changed'}
if($code -ne 0){Get-Content -LiteralPath $stderr;throw "Native game test failed: $code"}
Get-Content -LiteralPath (Join-Path $profile 'hp2vr-native.jsonl') -Tail 5

if($PlaySession){
    & python -B (Resolve-ProjectPath 'scripts\session_profiles.py') --run (Resolve-ProjectPath "logs\native-$stamp.run.json") --state $SessionState
    if($LASTEXITCODE -ne 0){throw 'Session profile retained, but continuation pointer was not updated'}
}
if($ManualExit){
    Write-Host 'VR-Sitzung beendet. Profil und Protokolle bleiben im Projektordner erhalten.'
    Write-Host ('Sitzungsprofil: '+$profile)
    return
}
& python -B (Resolve-ProjectPath 'scripts\check_native_trace.py') --run (Resolve-ProjectPath "logs\native-$stamp.run.json") --output (Resolve-ProjectPath "logs\native-$stamp.evidence.json")
if($LASTEXITCODE -ne 0){throw 'Independent native evidence check failed'}
