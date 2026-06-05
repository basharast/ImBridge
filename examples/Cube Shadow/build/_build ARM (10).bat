@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
call "%VCVARS%"

echo Compiling resources...
rc.exe /fo CubeShadow.res ..\CubeShadow.rc
if errorlevel 1 (
    echo Warning: Resource compilation failed, continuing without resources...
)

echo Compiling ARM32 Cube Shadow Payload...
cl.exe /O2 /EHsc ..\main.cpp /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\arm" user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib CubeShadow.res /OUT:CubeShadow.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause
