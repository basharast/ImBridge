@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
call "%VCVARS%"

echo Compiling resources...
rc.exe /fo Cube3D.res ..\Cube3D.rc
if errorlevel 1 (
    echo Warning: Resource compilation failed, continuing without resources...
)

set "WIN81_SDK=C:\Program Files (x86)\Windows Kits\8.1"
set "WIN81_LIB=%WIN81_SDK%\Lib\winv6.3\um\arm"
set "WIN81_INCLUDE=%WIN81_SDK%\Include\um;%WIN81_SDK%\Include\shared;%WIN81_SDK%\Include\winrt"

echo Compiling ARM D3D11 Payload for Windows 8.1...
cl.exe /O2 /D_WIN32_WINNT=0x0603 /DWINVER=0x0603 /I"%WIN81_SDK%\Include\um" /I"%WIN81_SDK%\Include\shared" ..\main.cpp /link /LIBPATH:"%WIN81_LIB%" user32.lib d3d11.lib dxgi.lib d3dcompiler.lib Cube3D.res /OUT:Cube3D.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause
