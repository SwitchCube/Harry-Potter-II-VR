@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Start-VR-Test.ps1"
if errorlevel 1 (
  echo.
  echo VR-Test nicht erfolgreich. Details stehen im Projektordner unter logs.
)
pause
