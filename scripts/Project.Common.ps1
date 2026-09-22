Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ProjectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))

function Assert-NoReparsePoint([string]$Path) {
    $current = [IO.Path]::GetFullPath($Path)
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Verknuepfung nicht erlaubt: $current"
            }
        }
        $parent = [IO.Path]::GetDirectoryName($current)
        if ($parent -eq $current) { break }
        $current = $parent
    }
}

function Resolve-ProjectPath([string]$Path) {
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $script:ProjectRoot $Path }
    $full = [IO.Path]::GetFullPath($Path)
    if ($full.Substring([IO.Path]::GetPathRoot($full).Length).Contains(':')) { throw 'Alternative Datenstroeme sind nicht erlaubt.' }
    if ($full -ne $script:ProjectRoot -and -not $full.StartsWith($script:ProjectRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Pfad ausserhalb der Projektwurzel: $full"
    }
    Assert-NoReparsePoint $full
    return $full
}

function New-ProjectDirectory([string]$Path) {
    $full = Resolve-ProjectPath $Path
    [IO.Directory]::CreateDirectory($full) | Out-Null
    return $full
}

function Write-NewProjectText([string]$Path, [string]$Text) {
    $full = Resolve-ProjectPath $Path
    New-ProjectDirectory ([IO.Path]::GetDirectoryName($full)) | Out-Null
    $stream = [IO.File]::Open($full, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
    return $full
}

function Get-ProjectConfig {
    $c = Get-Content -LiteralPath (Resolve-ProjectPath 'config\project.json') -Raw | ConvertFrom-Json
    if ($c.schema_version -ne 1) { throw 'Unbekanntes Konfigurationsschema' }
    Resolve-ProjectPath $c.game_root | Out-Null
    $exe = Resolve-ProjectPath $c.game_executable
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Spiel-EXE fehlt: $exe" }
    return $c
}

function Get-SafeFiles([string]$Path) {
    Assert-NoReparsePoint $Path
    if (-not (Test-Path -LiteralPath $Path)) { return }
    foreach ($item in Get-ChildItem -LiteralPath $Path -Force) {
        Assert-NoReparsePoint $item.FullName
        if ($item.PSIsContainer) { Get-SafeFiles $item.FullName } else { $item }
    }
}
