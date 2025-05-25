@echo off

:: NOTE: Intentionally not specifying 'setlocal' as we want the side-effects of calling 'vcvars' to affect the caller

:: Use vswhere.exe to find the Visual Studio installation path
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VS_INSTALL_PATH=%%i
)

if "%VS_INSTALL_PATH%"=="" (
    echo ERROR: Could not locate Visual Studio installation with required C++ tools using vswhere.exe
    exit /B 1
)

set VCVARS_PATH=%VS_INSTALL_PATH%\VC\Auxiliary\Build\vcvarsall.bat
if NOT EXIST "%VCVARS_PATH%" (
    echo ERROR: Could not locate vcvarsall.bat at %VCVARS_PATH%
    exit /B 1
)

set ARCH=%1
if /I "%ARCH%"=="x86" (
    REM Use the x64 toolchain
    set ARCH=x64_x86
)

call "%VCVARS_PATH%" %ARCH%
