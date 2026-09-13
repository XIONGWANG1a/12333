@echo off
REM Build simple_prot.sys kernel driver
REM Requires: WDK installed, run from VS Developer Command Prompt (x64)

set WDK_DIR=C:\Program Files (x86)\Windows Kits\10
set DRIVER_DIR=%~dp0
set OUT_DIR=%DRIVER_DIR%build

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM Pick the latest installed 10.x WDK version
set WDK_VER=
for /f "delims=" %%v in ('dir /b /ad "%WDK_DIR%\Include\10.*" 2^>nul ^| sort') do set WDK_VER=%%v
if not defined WDK_VER (
    echo Error: no WDK 10.x include tree found under "%WDK_DIR%\Include".
    exit /b 1
)

echo Building simple_prot.sys (WDK %WDK_VER%) ...

cl /c /O2 /W4 /wd4100 /GS /D _AMD64_ /D NOMINMAX ^
   /I "%DRIVER_DIR%inc" ^
   /I "%WDK_DIR%\Include\%WDK_VER%\km" ^
   /I "%WDK_DIR%\Include\%WDK_VER%\shared" ^
   /Fo"%OUT_DIR%\driver.obj" ^
   "%DRIVER_DIR%src\driver.c"

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

link /DRIVER /SUBSYSTEM:native /NXCOMPAT /ENTRY:DriverEntry ^
   /OUT:"%OUT_DIR%\simple_prot.sys" ^
   /LIBPATH:"%WDK_DIR%\Lib\%WDK_VER%\km\x64" ^
   ntoskrnl.lib hal.lib wdm.lib ^
   "%OUT_DIR%\driver.obj"

if errorlevel 1 (
    echo Link failed.
    exit /b 1
)

echo Build successful: %OUT_DIR%\simple_prot.sys
echo.
echo To install, run install_driver.bat as Admin.
