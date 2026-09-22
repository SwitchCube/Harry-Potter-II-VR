[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BuildDirectory)
. (Join-Path $PSScriptRoot 'Project.Common.ps1')
$build=Resolve-ProjectPath $BuildDirectory
$record=Get-Content -LiteralPath (Resolve-ProjectPath (Join-Path $build 'build.json')) -Raw|ConvertFrom-Json
$exe=Resolve-ProjectPath $record.executable
if((Get-FileHash -LiteralPath $exe).Hash -ne $record.executable_sha256){throw 'Runtime probe changed'}
$lock=Get-Content -LiteralPath (Resolve-ProjectPath 'config\research-sources.lock.json') -Raw|ConvertFrom-Json
$row=@($lock.files|Where-Object local_path -EQ 'external/openvr/bin/win32/openvr_api.dll')[0]
$dll=Resolve-ProjectPath $row.local_path
if((Get-FileHash -LiteralPath $dll).Hash -ne $row.sha256){throw 'OpenVR DLL changed'}
Write-Host 'Aktiver SteamVR-Test: SteamVR kann eigene externe Logs/Einstellungen schreiben. Keine Standardlaufzeit wird umgestellt.'
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$stdout=Resolve-ProjectPath "logs\runtime-$stamp.jsonl";$stderr=Resolve-ProjectPath "logs\runtime-$stamp.stderr.txt"
# Own the process handle from creation: Start-Process -PassThru can expose a
# null ExitCode after WaitForExit in Windows PowerShell 5.1.
$startInfo=[Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName=$exe
$startInfo.Arguments='"'+$dll+'"'
$startInfo.WorkingDirectory=$script:ProjectRoot
$startInfo.UseShellExecute=$false
$startInfo.CreateNoWindow=$true
$startInfo.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
$startInfo.RedirectStandardOutput=$true
$startInfo.RedirectStandardError=$true
$startInfo.EnvironmentVariables['TEMP']=New-ProjectDirectory 'cache\tmp'
$startInfo.EnvironmentVariables['TMP']=$startInfo.EnvironmentVariables['TEMP']
$p=[Diagnostics.Process]::new()
$p.StartInfo=$startInfo
try{
    if(-not $p.Start()){throw 'Runtime diagnostic could not be started'}
    # Drain both pipes concurrently so neither can block the child process.
    $outputTask=$p.StandardOutput.ReadToEndAsync()
    $errorTask=$p.StandardError.ReadToEndAsync()
    $timedOut=-not $p.WaitForExit(45000)
    if($timedOut){$p.Kill();$p.WaitForExit()}
    $code=$p.ExitCode
    Write-NewProjectText $stdout ($outputTask.GetAwaiter().GetResult()) | Out-Null
    Write-NewProjectText $stderr ($errorTask.GetAwaiter().GetResult()) | Out-Null
    if($timedOut){throw 'Runtime diagnostic timed out after 45 seconds'}
}finally{$p.Dispose()}
$report=[ordered]@{exit_code=$code;build=$BuildDirectory;trace=$stdout;stderr=$stderr;date_utc=[DateTime]::UtcNow.ToString('o');game_started=$false}
Write-NewProjectText "logs\runtime-$stamp.run.json" ($report|ConvertTo-Json)|Write-Output
Get-Content -LiteralPath $stdout
if($code -ne 0){Get-Content -LiteralPath $stderr;throw "Active runtime diagnostic failed: $code"}
