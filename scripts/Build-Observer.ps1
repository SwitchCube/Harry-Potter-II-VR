[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$compiler=Resolve-ProjectPath 'tools\zig-x86_64-windows-0.15.2\zig.exe'
& python -B (Resolve-ProjectPath 'scripts\setup_analysis.py') | Write-Host
if($LASTEXITCODE -ne 0){throw 'Analyseabhaengigkeit nicht verifiziert'}
& python -B (Resolve-ProjectPath 'scripts\prepare_observer.py') --check | Write-Host
if($LASTEXITCODE -ne 0){throw 'Observer-Zielbeschreibung ist veraltet oder Originalversion abweichend'}
& python -B (Resolve-ProjectPath 'scripts\prepare_pair_probe.py') --check | Write-Host
if($LASTEXITCODE -ne 0){throw 'Paar-Test-Zielbeschreibung ist veraltet'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$build=New-ProjectDirectory "build\observer-$stamp"
$oldEnv=@{}
foreach($name in @('TEMP','TMP','ZIG_GLOBAL_CACHE_DIR','ZIG_LOCAL_CACHE_DIR')){$oldEnv[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try{
    $env:TEMP=New-ProjectDirectory 'cache\tmp';$env:TMP=$env:TEMP
    $env:ZIG_GLOBAL_CACHE_DIR=New-ProjectDirectory 'cache\zig-global';$env:ZIG_LOCAL_CACHE_DIR=New-ProjectDirectory 'cache\zig-local'
    $source=Resolve-ProjectPath 'src\engine_observer.cpp';$header=Resolve-ProjectPath 'src\observed_target.h'
    $inputs=@('src\engine_observer.cpp','src\observed_target.h','src\pair_capture.h','src\pair_target.h') | ForEach-Object {[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Resolve-ProjectPath $_)).Hash}}
    $exe=Resolve-ProjectPath (Join-Path $build 'hp2vr-engine-observer.exe')
    $out=& $compiler c++ -target x86-windows-gnu -std=c++11 -O2 -municode $source -lbcrypt -lpsapi -o $exe 2>&1
    $code=$LASTEXITCODE
    $log=Write-NewProjectText "logs\build-observer-$stamp.log" (($out|Out-String)+"`nExitCode=$code`n")
    if($code -ne 0){throw "Observer-Build fehlgeschlagen: $log"}
    Write-NewProjectText (Join-Path $build 'build.json') ([ordered]@{target='original-engine-observer';executable=$exe.Substring($script:ProjectRoot.Length+1);source_sha256=(Get-FileHash -LiteralPath $source).Hash;target_header_sha256=(Get-FileHash -LiteralPath $header).Hash;inputs=$inputs;executable_sha256=(Get-FileHash -LiteralPath $exe).Hash;compiler_version=(& $compiler version|Out-String).Trim();target_triple='x86-windows-gnu';date_utc=[DateTime]::UtcNow.ToString('o');exit_code=$code}|ConvertTo-Json -Depth 5)|Out-Null
    Write-Output $build
}finally{foreach($name in $oldEnv.Keys){[Environment]::SetEnvironmentVariable($name,$oldEnv[$name],'Process')}}
