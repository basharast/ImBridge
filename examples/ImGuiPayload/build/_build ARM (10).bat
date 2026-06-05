@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
call "%VCVARS%"

echo Compiling ARM32 ImGui Payload...
cl.exe /O2 /EHsc /DUNICODE /D_UNICODE /I.. /I..\imgui ..\main.cpp ^
    ..\imgui\imgui.cpp ^
    ..\imgui\imgui_demo.cpp ^
    ..\imgui\imgui_draw.cpp ^
    ..\imgui\imgui_tables.cpp ^
    ..\imgui\imgui_widgets.cpp ^
    ..\imgui\backends\imgui_impl_win32.cpp ^
    ..\imgui\backends\imgui_impl_dx11.cpp ^
    -DIMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS ^
    -DIMGUI_DISABLE_UWP_DEFAULT_CLIPBOARD_FUNCTIONS ^
    -DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS ^
    -DIMGUI_IMPL_UWP_DISABLE_GAMEPAD ^
    -DIMGUI_DEFINE_MATH_OPERATORS ^
    -D_CRT_NONSTDC_NO_DEPRECATE ^
    -D_CRT_SECURE_NO_WARNINGS ^
    -DIMGUI_DEFINE_MATH_OPERATORS ^
    /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\arm" ^
    user32.lib gdi32.lib d3d11.lib dxgi.lib /OUT:ImGuiPayload.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful.
) else (
    echo Build failed.
)
pause
