[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$compiler = Resolve-ProjectPath 'tools\zig-x86_64-windows-0.15.2\zig.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Portable Toolchain fehlt. scripts\Setup-ProbeToolchain.ps1 ausfuehren.' }
$sourceLock=Get-Content -LiteralPath (Resolve-ProjectPath 'config\research-sources.lock.json') -Raw|ConvertFrom-Json
$header=@($sourceLock.files|Where-Object local_path -EQ 'external/openvr/headers/openvr.h')
if($header.Count -ne 1){throw 'OpenVR-Header fehlt im Quellen-Lock'}
if((Get-FileHash -LiteralPath (Resolve-ProjectPath $header[0].local_path) -Algorithm SHA256).Hash -ne $header[0].sha256){throw 'OpenVR-Header weicht vom Quellen-Lock ab'}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$build = New-ProjectDirectory "build\probe-$stamp"
$cache = New-ProjectDirectory 'cache\zig-global'
$localCache = New-ProjectDirectory 'cache\zig-local'
$temp = New-ProjectDirectory 'cache\tmp'
$oldEnv = @{}
foreach($name in @('TEMP','TMP','ZIG_GLOBAL_CACHE_DIR','ZIG_LOCAL_CACHE_DIR')) { $oldEnv[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $env:TEMP=$temp; $env:TMP=$temp; $env:ZIG_GLOBAL_CACHE_DIR=$cache; $env:ZIG_LOCAL_CACHE_DIR=$localCache
    $log = Resolve-ProjectPath "logs\build-probe-$stamp.log"
    $exe = Resolve-ProjectPath (Join-Path $build 'hp2vr-openvr-probe.exe')
    $source = Resolve-ProjectPath 'src\openvr_probe.cpp'
    $inc = Resolve-ProjectPath 'external\openvr\headers'
    $output = & $compiler c++ -target x86-windows-gnu -std=c++11 -O2 -municode -I $inc $source -o $exe 2>&1
    $code = $LASTEXITCODE
    Write-NewProjectText $log (($output | Out-String) + "`nExitCode=$code`n") | Out-Null
    if ($code -ne 0) { throw "C++ Build fehlgeschlagen: $log" }
    $record = [ordered]@{target='standalone-openvr-c-api-probe';executable=$exe.Substring($script:ProjectRoot.Length+1);
        source_sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash;
        executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
        compiler_version=(& $compiler version | Out-String).Trim();target_triple='x86-windows-gnu';
        engine_abi_compatibility='not established';built_utc=[DateTime]::UtcNow.ToString('o');build_exit_code=$code}
    Write-NewProjectText (Join-Path $build 'build.json') ($record | ConvertTo-Json) | Out-Null
    Write-Output $exe
} finally {
    foreach($name in $oldEnv.Keys) { [Environment]::SetEnvironmentVariable($name, $oldEnv[$name], 'Process') }
}
