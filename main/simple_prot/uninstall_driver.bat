@echo off
REM Uninstall simple_prot driver
REM Run as Administrator

net session >nul 2>&1
if errorlevel 1 (
    echo Error: run as Administrator.
    pause
    exit /b 1
)

echo Stopping service ...
sc stop SimpleProtBin >nul 2>&1
if errorlevel 1 (
    echo Note: service not stopped (may not be running).
)
sc delete SimpleProtBin >nul 2>&1

echo Removing driver file ...
del /F "%SystemRoot%\System32\drivers\simple_prot.sys" >nul 2>&1
if errorlevel 1 (
    echo Note: driver file still present (in use?).
)

echo Done.
pause
