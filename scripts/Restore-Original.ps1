[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Manifest,[Parameter(Mandatory=$true)][string]$Changes,[switch]$Apply)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
if($Apply -and @(Get-Process Game -ErrorAction SilentlyContinue).Count -gt 0){throw 'Vor Wiederherstellung das Spiel schliessen.'}
$argsList=@('-B',(Resolve-ProjectPath 'scripts\restore_original.py'),'--manifest',(Resolve-ProjectPath $Manifest),'--changes',(Resolve-ProjectPath $Changes))
if($Apply){$argsList+='--apply'}
& python @argsList
if($LASTEXITCODE -ne 0){throw "Wiederherstellung abgebrochen: Exit $LASTEXITCODE"}
