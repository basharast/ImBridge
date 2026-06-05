@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
call "%VCVARS%"

echo Compiling resources...
rc.exe /fo Cube3D.res ..\Cube3D.rc
if errorlevel 1 (
    echo Warning: Resource compilation failed, continuing without resources...
)

echo Compiling ARM32 D3D11 Payload...
cl.exe /O2 ..\main.cpp /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\arm" user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib Cube3D.res /OUT:Cube3D.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause
