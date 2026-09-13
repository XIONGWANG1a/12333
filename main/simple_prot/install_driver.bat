@echo off
REM Install simple_prot driver
REM Run as Administrator

net session >nul 2>&1
if errorlevel 1 (
    echo Error: run as Administrator.
    pause
    exit /b 1
)

set DRIVER_DIR=%~dp0
set SYS_FILE=%DRIVER_DIR%build\simple_prot.sys
set BINPATH=%SystemRoot%\System32\drivers

if not exist "%SYS_FILE%" (
    echo Error: %SYS_FILE% not found. Run build_driver.bat first.
    pause
    exit /b 1
)

echo Copying driver to System32\drivers ...
copy /Y "%SYS_FILE%" "%BINPATH%\simple_prot.sys"
if errorlevel 1 (
    echo Copy failed (is an old driver still loaded? run uninstall_driver.bat first).
    pause
    exit /b 1
)

echo Creating service ...
sc query SimpleProtBin >nul 2>&1
if errorlevel 1 (
    sc create SimpleProtBin type= kernel start= demand binPath= "%BINPATH%\simple_prot.sys"
    if errorlevel 1 (
        echo Service creation failed.
        pause
        exit /b 1
    )
)
sc start SimpleProtBin

if errorlevel 1 (
    echo Service start failed.
    echo Possible causes: driver not signed - run: bcdedit /set testsigning on, then reboot.
    echo Also check that the .sys actually built (run build_driver.bat).
) else (
    echo Driver installed and running.
)

pause
