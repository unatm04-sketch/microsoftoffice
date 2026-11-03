@echo off
rem OmniOffice derleme betigi.
rem Kullanim: build.bat [msvc-release|msvc-debug] [run]
setlocal

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc-release"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [HATA] vswhere bulunamadi. Visual Studio C++ araclarini kurun.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo [HATA] MSVC x64 araclari bulunamadi.
    exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1

if /i "%~2"=="run" start "" "%~dp0build\%PRESET%\src\app\OmniOffice.exe"
endlocal
