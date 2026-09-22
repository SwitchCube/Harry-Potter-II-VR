[CmdletBinding()]
param([switch]$RuntimeProbeOnly,[switch]$DirectBgrxUpload)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$cc=Resolve-ProjectPath 'tools\zig-x86_64-windows-0.15.2\zig.exe'
$lock=Get-Content -LiteralPath (Resolve-ProjectPath 'config\native-vr-dependencies.lock.json') -Raw|ConvertFrom-Json
foreach($row in $lock.files){if((Get-FileHash -LiteralPath (Resolve-ProjectPath $row.path)).Hash -ne $row.sha256){throw "Dependency changed: $($row.path)"}}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$build=New-ProjectDirectory "build\native-vr-$stamp"
$old=@{}
foreach($name in @('TEMP','TMP','ZIG_GLOBAL_CACHE_DIR','ZIG_LOCAL_CACHE_DIR')){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try{
    $env:TEMP=New-ProjectDirectory 'cache\tmp';$env:TMP=$env:TEMP
    $env:ZIG_GLOBAL_CACHE_DIR=New-ProjectDirectory 'cache\zig-global';$env:ZIG_LOCAL_CACHE_DIR=New-ProjectDirectory 'cache\zig-local'
    $common=@('c++','-target','x86-windows-gnu','-std=c++11','-O2','-s','-I',(Resolve-ProjectPath 'external\openvr\headers'))
    $exe=Resolve-ProjectPath (Join-Path $build 'hp2vr-runtime-probe.exe')
    $out=& $cc @common -municode (Resolve-ProjectPath 'src\vr_runtime_probe.cpp') -o $exe 2>&1
    $code=$LASTEXITCODE
    Write-NewProjectText "logs\build-native-vr-$stamp.log" (($out|Out-String)+"`nExitCode=$code")|Out-Null
    if($code -ne 0){throw 'Runtime probe build failed'}
    $streamProbe=Resolve-ProjectPath (Join-Path $build 'vr-streaming-probe.exe')
    & $cc @common -municode (Resolve-ProjectPath 'src\vr_streaming_probe.cpp') -o $streamProbe
    if($LASTEXITCODE -ne 0){throw 'Steam Link diagnostic utility compile failed'}
    $nativeDll=$null;$launcher=$null
    $nativeOptions=@();if($DirectBgrxUpload){$nativeOptions+='-DHP2VR_DIRECT_BGRX=1'}
    $test=Resolve-ProjectPath (Join-Path $build 'vr-math-tests.exe')
    & $cc @common -I (Resolve-ProjectPath 'src') (Resolve-ProjectPath 'tests\vr_math_tests.cpp') -o $test
    if($LASTEXITCODE -ne 0){throw 'VR math test compile failed'}
    & $test | Write-Host
    if($LASTEXITCODE -ne 0){throw 'VR math reference checks failed'}
    $capabilityTest=Resolve-ProjectPath (Join-Path $build 'vr-capabilities-tests.exe')
    & $cc @common -I (Resolve-ProjectPath 'src') (Resolve-ProjectPath 'tests/vr_capabilities_tests.cpp') -ldxgi -o $capabilityTest
    if($LASTEXITCODE -ne 0){throw 'GPU capability test compile failed'}
    & $capabilityTest | Write-Host
    if($LASTEXITCODE -ne 0){throw 'GPU capability checks failed'}
    $pixelTest=Resolve-ProjectPath (Join-Path $build 'vr-pixels-tests.exe')
    & $cc @common -msse2 -I (Resolve-ProjectPath 'src') (Resolve-ProjectPath 'tests\vr_pixels_tests.cpp') -o $pixelTest
    if($LASTEXITCODE -ne 0){throw 'VR pixel test compile failed'}
    & $pixelTest | Write-Host
    if($LASTEXITCODE -ne 0){throw 'VR pixel reference checks failed'}
    $hudTest=Resolve-ProjectPath (Join-Path $build 'vr-hud-capture-tests.exe')
    & $cc @common -msse2 -I (Resolve-ProjectPath 'src') (Resolve-ProjectPath 'tests/vr_hud_capture_tests.cpp') -lddraw -o $hudTest
    if($LASTEXITCODE -ne 0){throw 'HUD capture test compile failed'}
    & $hudTest | Write-Host
    if($LASTEXITCODE -ne 0){throw 'HUD target fallback checks failed'}
    $bindingTest=Resolve-ProjectPath (Join-Path $build 'vr-bindings-tests.exe')
    & $cc @common -municode -I (Resolve-ProjectPath 'src') (Resolve-ProjectPath 'tests/vr_bindings_tests.cpp') -o $bindingTest
    if($LASTEXITCODE -ne 0){throw 'Controller binding tests compile failed'}
    $fixture=New-ProjectDirectory (Join-Path $build 'binding-fixture')
    New-ProjectDirectory (Join-Path $fixture 'config') | Out-Null
    & $bindingTest $fixture | Write-Host
    if($LASTEXITCODE -ne 0){throw 'Controller binding checks failed'}
    $runtimeTest=Resolve-ProjectPath (Join-Path $build 'vr-runtime-tests.exe')
    & $cc @common -I (Resolve-ProjectPath 'src') (Resolve-ProjectPath 'tests/vr_runtime_tests.cpp') -o $runtimeTest
    if($LASTEXITCODE -ne 0){throw 'Frame lifecycle test compile failed'}
    & $runtimeTest | Write-Host
    if($LASTEXITCODE -ne 0){throw 'Frame lifecycle checks failed'}
    $inputPaths=@('src/vr_streaming_probe.cpp','tests/vr_runtime_tests.cpp','src\vr_runtime.h','src\vr_runtime_probe.cpp','src\vr_math.h','tests\vr_math_tests.cpp','tests/vr_bindings_tests.cpp','src\vr_pixels.h','tests\vr_pixels_tests.cpp')
    if(-not $RuntimeProbeOnly){
        $inputPaths+=@('src/vr_hud_capture.h','tests/vr_hud_capture_tests.cpp')
        & python -B (Resolve-ProjectPath 'scripts\prepare_observer.py') --check | Write-Host
        if($LASTEXITCODE -ne 0){throw 'Observer target check failed'}
        & python -B (Resolve-ProjectPath 'scripts\prepare_native_targets.py') --check | Write-Host
        if($LASTEXITCODE -ne 0){throw 'Native target check failed'}
        $objects=@()
        foreach($name in @('buffer','hook','trampoline','hde/hde32')){
            $obj=Resolve-ProjectPath (Join-Path $build (($name -replace '/','-')+'.o'))
            $out=& $cc cc -target x86-windows-gnu -O2 -c (Resolve-ProjectPath "external\minhook\src\$name.c") -o $obj 2>&1
            if($LASTEXITCODE -ne 0){$out|Write-Host;throw "MinHook compile failed: $name"}
            $objects+=$obj
        }
        $nativeDll=Resolve-ProjectPath (Join-Path $build 'hp2vr-native.dll')
        $out=& $cc @common @nativeOptions -msse2 -shared -I (Resolve-ProjectPath 'external\minhook\include') (Resolve-ProjectPath 'src\hp2vr_native.cpp') @objects -ld3d11 -ldxgi -lbcrypt -lpsapi -lgdi32 -o $nativeDll 2>&1
        $code=$LASTEXITCODE
        Write-NewProjectText "logs\build-native-dll-$stamp.log" (($out|Out-String)+"`nExitCode=$code")|Out-Null
        if($code -ne 0){throw "Native DLL compile failed: logs/build-native-dll-$stamp.log"}
        & python -B (Resolve-ProjectPath 'scripts\describe_native_dll.py') --dll $nativeDll --output (Join-Path $build 'native_launcher_target.h') | Write-Host
        if($LASTEXITCODE -ne 0){throw 'Native DLL export unavailable'}
        & python -B (Resolve-ProjectPath 'scripts\prepare_runtime_image.py') --build $build | Write-Host
        if($LASTEXITCODE -ne 0){throw 'Runtime image preparation failed'}
        $launcher=Resolve-ProjectPath (Join-Path $build 'hp2vr-native-launcher.exe')
        $out=& $cc @common -municode -DHP2VR_NATIVE_LAUNCHER=1 -I $build (Resolve-ProjectPath 'src\engine_observer.cpp') -lbcrypt -lpsapi -lgdi32 -o $launcher 2>&1
        $code=$LASTEXITCODE
        Write-NewProjectText "logs\build-native-launcher-$stamp.log" (($out|Out-String)+"`nExitCode=$code")|Out-Null
        if($code -ne 0){throw "Native launcher compile failed: logs/build-native-launcher-$stamp.log"}
        $flatLauncher=Resolve-ProjectPath (Join-Path $build 'hp2vr-flat-launcher.exe')
        $out=& $cc @common -municode (Resolve-ProjectPath 'src/flat_launcher.cpp') -lbcrypt -lgdi32 -o $flatLauncher 2>&1
        $code=$LASTEXITCODE
        Write-NewProjectText "logs/build-flat-launcher-$stamp.log" (($out|Out-String)+"`nExitCode=$code")|Out-Null
        if($code -ne 0){throw "Flat launcher compile failed: logs/build-flat-launcher-$stamp.log"}
        $inputPaths+=@('src/vr_capabilities.h','tests/vr_capabilities_tests.cpp','src/vr_compat.h','src/vr_locale.h','src/vr_portable.h','src/flat_launcher.cpp','scripts/Start-Spiel.ps1','scripts/shared_saves.py','scripts/menu_sessions.py','scripts/stage_test_profile.py')
        $inputPaths+=@('scripts\prepare_runtime_image.py','src\hp2vr_native.cpp','src\vr_native_api.h','src\vr_bridge.h','src\vr_profile.h','src\vr_audio.h','src\vr_window.h','src\vr_game_render.h','src\vr_game_input.h','src/vr_locomotion.h','src/vr_sword.h','src/vr_polish.h','src/vr_polish_math.h','src/vr_presentation.h','src/vr_vendor_panel.h','src/vr_gameplay.h','src/vr_gameplay_math.h','src/vr_gameplay_test.h','src/vr_lesson_test.h','src\vr_controls.h','src\vr_wand_pose.h','src\vr_hud.h','src\vr_health_math.h','src\vr_menu.h','src\vr_game_menu.h','src\vr_wand.h','src\vr_input.h','src\vr_settings.h','src\vr_panel.h','src\vr_bindings.h','src\vr_binding_menu.h','src\vr_status.h','src\vr_cinema.h','config\vr\actions_left.json','config\vr\bindings_touch_left.json','config\vr\actions.json','config\vr\bindings_touch.json','src\native_targets.h','src\native_inject.h','src\vr_frontend.h','src\vr_process_tree.h','src\engine_observer.cpp','src\observed_target.h','src\pair_capture.h','src\pair_target.h')
    }
    $inputPaths+=@(Get-ChildItem -LiteralPath (Resolve-ProjectPath 'config/vr') -Filter '*.json' | ForEach-Object {'config/vr/'+$_.Name})
    $inputs=$inputPaths | Sort-Object -Unique | ForEach-Object {[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Resolve-ProjectPath $_)).Hash}}
    $record=[ordered]@{target='active-runtime-probe';stream_probe=$streamProbe;stream_probe_sha256=(Get-FileHash -LiteralPath $streamProbe).Hash;executable=$exe;executable_sha256=(Get-FileHash -LiteralPath $exe).Hash;inputs=$inputs;date_utc=[DateTime]::UtcNow.ToString('o');render_bridge=$(if($DirectBgrxUpload){'direct-bgrx'}else{'opaque-bgra-sse2'})}
    if($nativeDll){$record.flat_launcher=$flatLauncher;$record.flat_launcher_sha256=(Get-FileHash -LiteralPath $flatLauncher).Hash;$record.target='native-game-integration';$record.dll=$nativeDll;$record.dll_sha256=(Get-FileHash -LiteralPath $nativeDll).Hash;$record.launcher=$launcher;$record.launcher_sha256=(Get-FileHash -LiteralPath $launcher).Hash}
    Write-NewProjectText (Join-Path $build 'build.json') ($record|ConvertTo-Json -Depth 5)|Out-Null
    Write-Output $build
}finally{foreach($name in $old.Keys){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}}
