@echo off
setlocal
"%~dp0HP2VR\python\python.exe" -B "%~dp0HP2VR\scripts\collect_diagnostics.py" %*
pause
endlocal
