[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$build=New-ProjectDirectory ('build/audio-diagnostic-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$cc=Resolve-ProjectPath 'tools/zig-x86_64-windows-0.15.2/zig.exe'
$old=@{}
foreach($name in @('TEMP','TMP','ZIG_GLOBAL_CACHE_DIR','ZIG_LOCAL_CACHE_DIR')){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try{
 $env:TEMP=New-ProjectDirectory 'cache/tmp';$env:TMP=$env:TEMP
 $env:ZIG_GLOBAL_CACHE_DIR=New-ProjectDirectory 'cache/zig-global';$env:ZIG_LOCAL_CACHE_DIR=New-ProjectDirectory 'cache/zig-local'
 $source=Resolve-ProjectPath 'src/audio_endpoint_probe.cpp'
 $exe=Resolve-ProjectPath (Join-Path $build 'audio-endpoint-probe.exe')
 & $cc c++ -target x86-windows-gnu -std=c++11 -O2 -municode -I (Resolve-ProjectPath 'external/openvr/headers') $source -lole32 -luuid -o $exe
 if($LASTEXITCODE -ne 0){throw 'Read-only audio diagnostic compile failed'}
 $inputs=@('src/audio_endpoint_probe.cpp','src/vr_runtime.h','external/openvr/headers/openvr_capi.h')|ForEach-Object {[ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Resolve-ProjectPath $_)).Hash}}
 Write-NewProjectText (Join-Path $build 'build.json') ([ordered]@{executable=$exe;sha256=(Get-FileHash -LiteralPath $exe).Hash;inputs=$inputs;read_only=$true}|ConvertTo-Json -Depth 4)|Out-Null
 Write-Output $exe
}finally{foreach($name in $old.Keys){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}}
