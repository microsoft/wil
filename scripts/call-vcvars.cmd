@echo off

:: NOTE: Intentionally not specifying 'setlocal' as we want the side-effects of calling 'vcvars' to affect the caller

:: NOTE: This is primarily intended to be used by the bulid pipelines, hence the hard-coded paths, and might not be
::       generally useful. The following check is intended to help diagnose such possible issues
if NOT EXIST "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    echo ERROR: Could not locate 'vcvars' batch file. This script is intended to be run from a build machine & exit /B 1
)

if /I "%1"=="x86" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
) else if /I "%1"=="x64" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
) else echo ERROR: Requires one of 'x86' or 'x64' & exit /B 1
