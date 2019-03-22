@echo off
setlocal EnableDelayedExpansion
:: init.cmd [compiler] [generator] [type]
:: Where:
::      compiler = clang | msvc
::      generator = ninja | msbuild
::      type = debug | release | relwithdebinfo | minsizerel

set BUILD_ROOT=%~dp0\..\build
mkdir %BUILD_ROOT% > NUL 2>&1

:: Check for target compiler
set COMPILER_ID=
if "%1"=="clang" (
    set COMPILER_ID=clang
    shift
) else if "%1"=="msvc" (
    set COMPILER_ID=msvc
    shift
)

:: Check for a generator
set CMAKE_ARGS=
if "%1"=="msbuild" (
    :: MSBUILD is the default
    if [%COMPILER_ID%]==[clang] (
        echo ERROR: Use of msbuild is not compatible with using Clang as the compiler
        goto :eof
    )
    set COMPILER_ID=msvc
    shift
) else (
    if "%1"=="ninja" shift
    set CMAKE_ARGS=%CMAKE_ARGS% -G Ninja
)

if not defined COMPILER_ID (
    set COMPILER_ID=clang
)

:: Check for a build type
if "%1"=="release" (
    set BUILD_TYPE=release
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release
    shift
) else if "%1"=="relwithdebinfo" (
    set BUILD_TYPE=relwithdebinfo
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=RelWithDebInfo
    shift
) else if "%1"=="minsizerel" (
    set BUILD_TYPE=minsizerel
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=MinSizeRel
    shift
) else (
    if "%1"=="debug" shift
    set BUILD_TYPE=debug
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Debug
)

if [%1] NEQ [] (
    echo ERROR: Unrecognized argument %1
    goto :eof
)

:: Let CMake know about the compiler we are using
if %COMPILER_ID%==clang (
    set CC=clang-cl
    set CXX=clang-cl
) else (
    set CC=cl
    set CXX=cl
)

:: E.g. the build folder will be something like "build/clang64debug"
if "%Platform%"=="x64" (
    set BUILD_ARCH=64
) else if "%Platform%"=="x86" (
    set BUILD_ARCH=32
) else if [%Platform%]==[] (
    echo ERROR: This script must be run from a Visual Studio command window
    goto :eof
) else (
    echo ERROR: Unrecognized platform %Platform%
    goto :eof
)

set BUILD_DIR=%BUILD_ROOT%\%COMPILER_ID%%BUILD_ARCH%%BUILD_TYPE%
mkdir %BUILD_DIR% > NUL 2>&1

pushd %BUILD_DIR%
cmake %CMAKE_ARGS% ..\..
popd
