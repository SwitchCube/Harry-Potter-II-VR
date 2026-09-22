@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Start-VR.ps1" -FreshSession
if errorlevel 1 pause
