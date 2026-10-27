@echo off
setlocal
cd /d "%~dp0"
set "BS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%BS_VSWHERE%" goto missing
for /f "usebackq tokens=*" %%i in (`"%BS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BS_VS=%%i"
if not defined BS_VS goto missing
call "%BS_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\bin mkdir build\bin
if not exist build\obj mkdir build\obj
cl /nologo /std:c++17 /EHsc /O2 /W4 /DUNICODE /D_UNICODE /DNOMINMAX /Ivendor\imgui /Ivendor\imgui\backends src\main.cpp src\render\renderer.cpp src\render\alpha_validation.cpp src\platform\window.cpp vendor\imgui\imgui.cpp vendor\imgui\imgui_draw.cpp vendor\imgui\imgui_tables.cpp vendor\imgui\imgui_widgets.cpp vendor\imgui\backends\imgui_impl_win32.cpp vendor\imgui\backends\imgui_impl_dx11.cpp /Fobuild\obj\ /Febuild\bin\VZX-CHEAT-BS.exe /link dcomp.lib d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib user32.lib gdi32.lib imm32.lib
if errorlevel 1 exit /b 1
copy /y config\layout.ini build\bin\layout.ini >nul
exit /b 0

:missing
echo Instale Visual Studio Build Tools com C++ x64 e Windows SDK.
exit /b 1
