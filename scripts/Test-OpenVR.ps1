[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BuildDirectory)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$build = Resolve-ProjectPath $BuildDirectory
$record = Get-Content -LiteralPath (Resolve-ProjectPath (Join-Path $build 'build.json')) -Raw | ConvertFrom-Json
$exe = Resolve-ProjectPath $record.executable
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $record.executable_sha256) { throw 'Probe-Binaerdatei geaendert' }
$lock = Get-Content -LiteralPath (Resolve-ProjectPath 'config\research-sources.lock.json') -Raw | ConvertFrom-Json
$row = @($lock.files | Where-Object local_path -EQ 'external/openvr/bin/win32/openvr_api.dll')
if ($row.Count -ne 1) { throw 'OpenVR DLL fehlt im Quellen-Lock' }
$dll = Resolve-ProjectPath $row[0].local_path
if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $row[0].sha256) { throw 'OpenVR DLL-Pruefsumme abweichend' }
Write-Host 'Externe 32-Bit-Diagnose: kein Spielstart, kein VR_Init, kein Tracking-/Bildtest.'
Write-Host 'Die fremde OpenVR-DLL kann eigene Daten ausserhalb der Projektwurzel anlegen.'
$result = & $exe $dll 2>&1
$code = $LASTEXITCODE
$report = [ordered]@{date_utc=[DateTime]::UtcNow.ToString('o');exit_code=$code;build=$record;output=($result|Out-String).Trim()}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
Write-NewProjectText "logs\openvr-probe-$stamp.json" ($report | ConvertTo-Json -Depth 6) | Write-Output
$result | Write-Output
if ($code -ne 0) { throw "OpenVR-Probe fehlgeschlagen: Exit $code" }
