[CmdletBinding()]
param([ValidateRange(0,60)][int]$ProbeSeconds = 0)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$config = Get-ProjectConfig
$fingerprint = Get-Content -LiteralPath (Resolve-ProjectPath 'config\target-fingerprint.json') -Raw | ConvertFrom-Json
foreach($file in $fingerprint.files) {
    $path=Resolve-ProjectPath $file.path
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) {
        throw "Referenzdatei weicht ab: $($file.path). Vor Start neue Version untersuchen."
    }
}
$exe = Resolve-ProjectPath $config.game_executable
if (@(Get-Process -Name Game -ErrorAction SilentlyContinue).Count -gt 0) { throw 'Eine Game.exe laeuft bereits. Kein paralleler Referenzstart.' }
Write-Host 'Nicht-VR-Start mit unveraenderten Spieldateien.'
Write-Host 'Das Originalspiel schreibt Konfiguration/Spielstaende im externen Benutzerprofil; Windows kann zusaetzliche Systemdaten schreiben.'
$python = (Get-Command python -ErrorAction Stop).Source
& $python -B (Resolve-ProjectPath 'scripts\backup_original.py') --reason 'Before flat start'
if ($LASTEXITCODE -ne 0) { throw 'Sicherung fehlgeschlagen. Spielstart abgebrochen.' }
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $exe
$start.WorkingDirectory = Split-Path -Parent $exe
$start.UseShellExecute = $false
$start.EnvironmentVariables['TEMP'] = New-ProjectDirectory 'cache\tmp'
$start.EnvironmentVariables['TMP'] = Resolve-ProjectPath 'cache\tmp'
$process = [Diagnostics.Process]::Start($start)
$report=[ordered]@{date_utc=[DateTime]::UtcNow.ToString('o');executable=$exe;working_directory=$start.WorkingDirectory;
    arguments='';pid=$process.Id;probe_seconds=$ProbeSeconds;vr=$false;visual_check='nicht getestet';headset_check='nicht getestet'}
if ($ProbeSeconds -gt 0) {
    $exited = $process.WaitForExit($ProbeSeconds * 1000)
    $process.Refresh()
    $report['exited_during_probe']=$exited
    if ($exited) { $report['exit_code']=$process.ExitCode } else {
        $report['window_title']=$process.MainWindowTitle
        $report['responding']=$process.Responding
        try { $report['loaded_game_modules']=@($process.Modules | Where-Object {$_.FileName.StartsWith((Split-Path -Parent $exe)+'\',[StringComparison]::OrdinalIgnoreCase)} | Select-Object ModuleName,FileName) }
        catch { $report['module_query_error']=$_.Exception.Message }
        $report['close_requested']=$process.CloseMainWindow()
        $report['closed_after_request']=$process.WaitForExit(10000)
        if ($process.HasExited) { $report['exit_code']=$process.ExitCode }
        else { $report['manual_action']="Spiel/Testfenster PID $($process.Id) manuell schliessen; kein erzwungenes Beenden." }
    }
}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
Write-NewProjectText "logs\flat-start-$stamp.json" ($report | ConvertTo-Json -Depth 6) | Write-Output
$report | ConvertTo-Json -Depth 6 | Write-Output
if($report.Contains('exit_code') -and $report['exit_code'] -ne 0){throw "Spielprozess meldet Exit $($report['exit_code'])"}
