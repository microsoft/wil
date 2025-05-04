@echo off
setlocal
setlocal EnableDelayedExpansion

:init
    set SOURCE_DIR=%~dp0\..
    set COMPILER=
    set BUILD_TYPE=
    set BUILD_ARCH=
    set DO_CMAKE_INIT=1

    if "%Platform%"=="" (
        echo ERROR: This script should be run from a ... Tools Command Prompt for VS 2022
        exit /B 1
    )

    goto :parse

:usage
    echo Usage: build.cmd [options]
    echo Options:
    echo   -c, --compiler ^<clang^|msvc^>   Compiler to use (default: clang)
    echo   -a, --arch ^<x86^|x64^|arm64^>   Architecture to build for (default: %Platform%)
    echo   -t, --type ^<release^|debug^|relwithdebinfo^|minsizerel^> Build type (default: debug)
    echo   -s, --skip-init                  Skips the CMake cache generation step
    echo   --help                           Show this help message
    exit /B 1

:parse
    set BUILD_TYPE_ARG=
    set COMPILER_ARG=
    set BUILD_ARCH_ARG=
    set DO_CMAKE_INIT_ARG=

    if /I "%~1"=="" goto :execute
    if /I "%~1"=="--help" call :usage & goto :eof

    if /I "%~1"=="-c" set COMPILER_ARG=1
    if /I "%~1"=="--compiler" set COMPILER_ARG=1
    if "%COMPILER_ARG%"=="1" (
        set COMPILER_TAG=
        if /I "%~2"=="clang" set COMPILER_TAG=clang
        if /I "%~2"=="msvc" set COMPILER_TAG=msvc
        if "!COMPILER_TAG!"=="" echo ERROR: Unrecognized/missing compiler %~2 & call :usage & exit /B 1
        set COMPILER=%COMPILER% !COMPILER_TAG!

        shift
        shift
        goto :parse
    )

    if /I "%~1"=="-s" set DO_CMAKE_INIT_ARG=1
    if /I "%~1"=="--skip-init" set DO_CMAKE_INIT_ARG=1
    if "%DO_CMAKE_INIT_ARG%"=="1" (
        set DO_CMAKE_INIT=0
        shift
        goto :parse
    )

    if /I "%~1"=="-a" set BUILD_ARCH_ARG=1
    if /I "%~1"=="--arch" set BUILD_ARCH_ARG=1
    if "%BUILD_ARCH_ARG%"=="1" (
        set BUILD_ARCH_TAG=
        if /I "%~2"=="x86" set BUILD_ARCH_TAG=x86
        if /I "%~2"=="x64" set BUILD_ARCH_TAG=x64
        if /I "%~2"=="arm64" set BUILD_ARCH_TAG=arm64
        if "!BUILD_ARCH_TAG!"=="" echo ERROR: Unrecognized/missing architecture %~2 & call :usage & exit /B 1
        set BUILD_ARCH=%BUILD_ARCH% !BUILD_ARCH_TAG!

        shift
        shift
        goto :parse
    )

    if /I "%~1"=="-t" set BUILD_TYPE_ARG=1
    if /I "%~1"=="--type" set BUILD_TYPE_ARG=1
    if "%BUILD_TYPE_ARG%"=="1" (        
        set BUILD_TYPE_TAG=
        if /I "%~2"=="debug" set BUILD_TYPE_TAG=debug
        if /I "%~2"=="release" set BUILD_TYPE_TAG=release
        if /I "%~2"=="relwithdebinfo" set BUILD_TYPE_TAG=relwithdebinfo
        if /I "%~2"=="minsizerel" set BUILD_TYPE_TAG=minsizerel
        if "!BUILD_TYPE_TAG!"=="" echo ERROR: Unrecognized/missing build type %~2 & call :usage & exit /B 1
        set BUILD_TYPE=%BUILD_TYPE% !BUILD_TYPE_TAG!

        shift
        shift
        goto :parse
    )

    echo ERROR: Unrecognized argument %~1 & call :usage & exit /B 1

:execute
    if "%COMPILER%"=="" set COMPILER=clang

    if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

    if "%BUILD_ARCH%"=="" set BUILD_ARCH=%Platform%

    if "%DO_CMAKE_INIT%"=="1" (
        for %%c in (%COMPILER%) do (
            echo INFO: Generating CMake cache for %COMPILER% %BUILD_ARCH% %BUILD_TYPE%
            call %SOURCE_DIR%\scripts\init.cmd -c %COMPILER% -a %BUILD_ARCH%
            if !ERRORLEVEL! NEQ 0 ( goto :eof )
        )
    ) else (
        echo INFO: Skipping CMake cache generation
    )

    for %%c in (%COMPILER%) do (
        for %%b in (%BUILD_TYPE%) do (
            echo INFO: Building for %BUILD_ARCH% %%c %%b
            pushd %SOURCE_DIR%
            cmake --build --preset %BUILD_ARCH%-%%c-%%b
            popd
            if !ERRORLEVEL! NEQ 0 ( 
                echo ERROR: Failed, error level !ERRORLEVEL!
                goto :eof 
            )
        )
    )

    echo All build completed successfully!
    goto :eof