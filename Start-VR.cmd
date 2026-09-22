@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Start-VR.ps1"
if errorlevel 1 (
  echo.
  echo VR-Start oder Sitzung nicht erfolgreich. Details stehen im Projektordner unter logs.
)
pause
