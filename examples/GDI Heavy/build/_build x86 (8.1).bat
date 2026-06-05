@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
call "%VCVARS%"

echo Compiling resources...
rc.exe /fo "GDI Heavy.res" "..\GDI Heavy.rc"
if errorlevel 1 (
    echo Warning: Resource compilation failed, continuing without resources...
)

set "WIN81_SDK=C:\Program Files (x86)\Windows Kits\8.1"
set "WIN81_LIB=%WIN81_SDK%\Lib\winv6.3\um\x86"

echo Compiling x86 GDI Heavy Payload for Windows 8.1...
cl.exe /O2 /EHsc /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0603 /DWINVER=0x0603 /I"%WIN81_SDK%\Include\um" /I"%WIN81_SDK%\Include\shared" ..\main.cpp /link /LIBPATH:"%WIN81_LIB%" user32.lib gdi32.lib wininet.lib psapi.lib ole32.lib advapi32.lib xaudio2.lib "GDI Heavy.res" /OUT:GDIHeavy.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause
