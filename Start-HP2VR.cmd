@echo off
setlocal
"%~dp0HP2VR\python\python.exe" -B "%~dp0HP2VR\scripts\portable_launcher.py" %*
if errorlevel 1 pause
endlocal
