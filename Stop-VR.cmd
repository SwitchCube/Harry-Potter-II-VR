@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Stop-VR.ps1"
if errorlevel 1 pause
