@echo off
setlocal
set WDK_DIR=C:\Program Files (x86)\Windows Kits\10
set DDK=%WDK_DIR%\Include\10.0.22621.0\km
set LIB=%WDK_DIR%\Lib\10.0.22621.0\km\x64
set OUT=bin

if not exist %OUT% mkdir %OUT%

cl /nologo /kernel /W4 /WX /GS- /Gy /O2 /Oi /Oy- ^
   /DAMD64 /D_NTKERNEL_ /D_WIN64 ^
   /I%DDK% /Iinc ^
   /c src\driver.c /Fo%OUT%\driver.obj

ml64 /nologo /c /Cx src\vmx_asm.asm /Fo%OUT%\vmx_asm.obj

link /nologo /out:%OUT%\SimpleProt.sys /DRIVER /SUBSYSTEM:NATIVE ^
     /ENTRY:DriverEntry ^
     /LIBPATH:%LIB% ^
     %OUT%\driver.obj %OUT%\vmx_asm.obj ^
     ntoskrnl.lib bufferoverflowfastfailk.lib hal.lib wdmsec.lib

if errorlevel 1 (echo BUILD FAILED & exit /b 1)
signtool sign /v /fd sha256 /a %OUT%\SimpleProt.sys
echo BUILD OK: %OUT%\SimpleProt.sys
endlocal