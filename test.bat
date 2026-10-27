@echo off
setlocal
cd /d "%~dp0"
set "BS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%BS_VSWHERE%" goto missing
for /f "usebackq tokens=*" %%i in (`"%BS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BS_VS=%%i"
if not defined BS_VS goto missing
call "%BS_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\tests mkdir build\tests
if not exist build\obj\tests mkdir build\obj\tests
for %%T in (projection_tests life_filter_tests recoil_tests team_filter_tests aim_target_tests) do (
cl /nologo /std:c++17 /EHsc /O2 /W4 /DNOMINMAX /Ivendor\imgui tests\%%T.cpp /Fobuild\obj\tests\%%T.obj /Febuild\tests\%%T.exe
if errorlevel 1 exit /b 1
build\tests\%%T.exe
if errorlevel 1 exit /b 1
)
exit /b 0

:missing
echo Instale Visual Studio Build Tools com C++ x64 e Windows SDK.
exit /b 1


