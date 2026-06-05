@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
call "%VCVARS%"

echo Compiling resources...
rc.exe /fo "GDI Heavy.res" "..\GDI Heavy.rc"
if errorlevel 1 (
    echo Warning: Resource compilation failed, continuing without resources...
)

echo Compiling ARM32 GDI Heavy Payload...
cl.exe /O2 /EHsc /DUNICODE /D_UNICODE ..\main.cpp /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\arm" user32.lib gdi32.lib wininet.lib psapi.lib ole32.lib advapi32.lib xaudio2.lib "GDI Heavy.res" /OUT:GDIHeavy.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause
