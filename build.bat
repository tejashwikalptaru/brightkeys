@echo off
rem Builds build\BrightKeys.exe with MSVC + CMake + Ninja (all included with Visual Studio's C++ workload).
rem Usage: build.bat [version] [Release^|Debug]      e.g.  build.bat 1.2.3
setlocal
cd /d "%~dp0"

set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=0.0.0"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Release"

where cl >nul 2>nul
if not errorlevel 1 goto :build

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :novs
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul

:build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% -DBK_VERSION=%VERSION% || exit /b 1
cmake --build build || exit /b 1
echo.
echo Built build\BrightKeys.exe (v%VERSION%, %CONFIG%)
exit /b 0

:novs
echo MSVC not found. Install Visual Studio (or Build Tools) with the "Desktop development with C++" workload.
exit /b 1
