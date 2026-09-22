[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$version = '0.15.2'
$expected = '3a0ed1e8799a2f8ce2a6e6290a9ff22e6906f8227865911fb7ddedc3cc14cb0c'
$archive = Resolve-ProjectPath "cache\zig-x86_64-windows-$version.zip"
$destination = Resolve-ProjectPath "tools\zig-x86_64-windows-$version"
New-ProjectDirectory 'cache' | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri "https://ziglang.org/download/$version/zig-x86_64-windows-$version.zip" -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Zig SHA256 stimmt nicht.' }
& python -B (Resolve-ProjectPath 'scripts\extract_portable.py')
if($LASTEXITCODE -ne 0){throw 'Toolchain-Entpacken fehlgeschlagen; vorhandene Dateien bleiben erhalten.'}
$lockPath=Resolve-ProjectPath 'config\probe-toolchain.lock.json'
if(Test-Path -LiteralPath $lockPath){
    $existing=Get-Content -LiteralPath $lockPath -Raw|ConvertFrom-Json
    if($existing.version -ne $version -or $existing.sha256 -ne $expected){throw 'Toolchain-Lock stimmt nicht mit dem Setup ueberein.'}
    Write-Output 'Vorhandene Toolchain gegen das gepruefte Archiv bestaetigt.'
    return
}
Write-NewProjectText 'config\probe-toolchain.lock.json' ([ordered]@{
    name='Zig';version=$version;url="https://ziglang.org/download/$version/zig-x86_64-windows-$version.zip";
    sha256=$expected;purpose='Standalone C++ x86 diagnostic only; not an approved HP2 C++ ABI toolchain';
    retrieved_utc=[DateTime]::UtcNow.ToString('o')
} | ConvertTo-Json) | Write-Output
