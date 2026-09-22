[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
& (Resolve-ProjectPath 'scripts\Inspect-Environment.ps1')
if(-not $?){throw 'Umgebungsdiagnose fehlgeschlagen'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
& python -B (Resolve-ProjectPath 'scripts\inspect_pe.py') "logs\pe-inventory-$stamp.json"
if($LASTEXITCODE -ne 0){throw 'PE-Analyse fehlgeschlagen'}
Write-Host 'Berichte bleiben lokal unter logs. Kein automatischer Upload; Spielstaende und Assets werden nicht paketiert.'
