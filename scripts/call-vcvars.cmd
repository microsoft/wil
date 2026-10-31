@echo off

REM NOTE: Intentionally not specifying 'setlocal' as we want the side-effects of calling 'vcvars' to affect the caller

set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
set _VCVARSALL=
for /f "delims=" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvarsall.bat') do set _VCVARSALL=%%i

if "%_VCVARSALL%"=="" echo ERROR: Unable to locate vcvarsall.bat & exit /B 1

set _ARCH=%1
if "%_ARCH%"=="" (
    set _ARCH=x64
    if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" set _ARCH=arm64
    if /I "%PROCESSOR_ARCHITECTURE%"=="x86" set _ARCH=x86
)
if /I "%_ARCH%"=="x86" (
    REM Use the x64 toolchain
    set _ARCH=x64_x86
)

call "%_VCVARSALL%" %_ARCH%
