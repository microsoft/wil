@echo off
setlocal
setlocal EnableDelayedExpansion

:: Globals
set BUILD_ROOT=%~dp0\..\build

goto :init

:usage
    echo USAGE:
    echo     init.cmd [--help] [-c^|--compiler ^<clang^|msvc^>] [-g^|--generator ^<ninja^|msbuild^>] [--fast]
    echo         [-b^|--build-type ^<debug^|release^|relwithdebinfo^|minsizerel^>] [-v^|--version X.Y.Z]
    goto :eof

:init
    :: Initialize values as empty so that we can identify if we are using defaults later for error purposes
    set COMPILER=
    set GENERATOR=
    set BUILD_TYPE=
    set CMAKE_ARGS=
    set BITNESS=
    set VERSION=
    set FAST_BUILD=0

:parse
    if "%~1"=="" goto :execute

    if "%~1"=="--help" call :usage & goto :eof

    set COMPILER_SET=0
    if "%~1"=="-c" set COMPILER_SET=1
    if "%~1"=="--compiler" set COMPILER_SET=1
    if %COMPILER_SET%==1 (
        if "%COMPILER%" NEQ "" echo ERROR: Compiler already specified & exit /B 1

        if "%~2"=="clang" set COMPILER=clang
        if "%~2"=="msvc" set COMPILER=msvc
        if "!COMPILER!"=="" echo ERROR: Unrecognized/missing compiler %~2 & exit /B 1

        shift
        shift
        goto :parse
    )

    set GENERATOR_SET=0
    if "%~1"=="-g" set GENERATOR_SET=1
    if "%~1"=="--generator" set GENERATOR_SET=1
    if %GENERATOR_SET%==1 (
        if "%GENERATOR%" NEQ "" echo ERROR: Generator already specified & exit /B 1

        if "%~2"=="ninja" set GENERATOR=ninja
        if "%~2"=="msbuild" set GENERATOR=msbuild
        if "!GENERATOR!"=="" echo ERROR: Unrecognized/missing generator %~2 & exit /B 1

        shift
        shift
        goto :parse
    )

    set BUILD_TYPE_SET=0
    if "%~1"=="-b" set BUILD_TYPE_SET=1
    if "%~1"=="--build-type" set BUILD_TYPE_SET=1
    if %BUILD_TYPE_SET%==1 (
        if "%BUILD_TYPE%" NEQ "" echo ERROR: Build type already specified & exit /B 1

        if "%~2"=="debug" set BUILD_TYPE=debug
        if "%~2"=="release" set BUILD_TYPE=release
        if "%~2"=="relwithdebinfo" set BUILD_TYPE=relwithdebinfo
        if "%~2"=="minsizerel" set BUILD_TYPE=minsizerel
        if "!BUILD_TYPE!"=="" echo ERROR: Unrecognized/missing build type %~2 & exit /B 1

        shift
        shift
        goto :parse
    )

    set VERSION_SET=0
    if "%~1"=="-v" set VERSION_SET=1
    if "%~1"=="--version" set VERSION_SET=1
    if %VERSION_SET%==1 (
        if "%VERSION%" NEQ "" echo ERROR: Version alread specified & exit /B 1
        if "%~2"=="" echo ERROR: Version string missing & exit /B 1

        set VERSION=%~2

        shift
        shift
        goto :parse
    )

    if "%~1"=="--fast" (
        if %FAST_BUILD% NEQ 0 echo ERROR: Fast build already specified
        set FAST_BUILD=1
        set CMAKE_ARGS=%CMAKE_ARGS% -DFAST_BUILD=ON

        shift
        goto :parse
    )

    echo ERROR: Unrecognized argument %~1
    exit /B 1

:execute
    :: Check for conflicting arguments
    if "%COMPILER%"=="clang" (
        if "%GENERATOR%"=="msbuild" echo ERROR: Cannot use Clang with MSBuild & exit /B 1
    )

    :: Select defaults
    if "%GENERATOR%"=="" set GENERATOR=ninja
    if %GENERATOR%==msbuild set COMPILER=msvc

    if "%COMPILER%"=="" set COMPILER=clang

    if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

    :: Formulate CMake arguments
    if %GENERATOR%==ninja set CMAKE_ARGS=%CMAKE_ARGS% -G Ninja

    if %COMPILER%==clang set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
    if %COMPILER%==msvc set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl

    if %BUILD_TYPE%==debug set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Debug
    if %BUILD_TYPE%==release set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release
    if %BUILD_TYPE%==relwithdebinfo set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=RelWithDebInfo
    if %BUILD_TYPE%==minsizerel set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=MinSizeRel

    if "%VERSION%" NEQ "" set CMAKE_ARGS=%CMAKE_ARGS% -DWIL_BUILD_VERSION=%VERSION%

    :: Figure out the platform
    if "%Platform%"=="" echo ERROR: This script must be run from a Visual Studio command window & exit /B 1
    if "%Platform%"=="x86" (
        set BITNESS=32
        if %COMPILER%==clang set CFLAGS=-m32 & set CXXFLAGS=-m32
    )
    if "%Platform%"=="x64" set BITNESS=64
    if "%BITNESS%"=="" echo ERROR: Unrecognized/unsupported platform %Platform% & exit /B 1

    :: Set up the build directory
    set BUILD_DIR=%BUILD_ROOT%\%COMPILER%%BITNESS%%BUILD_TYPE%
    mkdir %BUILD_DIR% > NUL 2>&1

    pushd %BUILD_DIR%
    echo Using compiler....... %COMPILER%
    echo Using architecture... %Platform%
    echo Using build type..... %BUILD_TYPE%
    echo Using build root..... %CD%
    echo.
    cmake %CMAKE_ARGS% ..\..
    popd

    goto :eof
