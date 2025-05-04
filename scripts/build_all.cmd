@echo off
setlocal
setlocal EnableDelayedExpansion

set SOURCE_DIR=%~dp0\..
set BUILD_ARCH=%Platform%

if "%1"=="-a" (
    set BUILD_ARCH=%2
    shift
    shift
) else if "%1"=="--arch" (
    set BUILD_ARCH=%2
    shift
    shift
) else (
    echo Invalid argument %1
    echo Usage: build_all.cmd [-a^|--arch ^<x86^|x64^|arm64^>]
    exit /B 1
)

if "%BUILD_ARCH%"=="" (
    echo ERROR: The build_all.cmd script must be run from a Visual Studio command window
    exit /B 1
)

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        call :build %%c %%b
            if !ERRORLEVEL! NEQ 0 ( goto :eof )
        )
    )

echo All build completed successfully!

goto :eof

:: build [compiler] [type]
:build
echo INFO: Building for %BUILD_ARCH% %1 %2
pushd %SOURCE_DIR%
cmake --build --preset %BUILD_ARCH%-%1-%2
set EXIT_CODE=%ERRORLEVEL%
popd
exit /B %EXIT_CODE%
