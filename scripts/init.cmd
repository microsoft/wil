@echo off
setlocal
setlocal EnableDelayedExpansion

:: Globals
set SOURCE_DIR=%~dp0\..

goto :init

:usage
    echo USAGE:
    echo     init.cmd [--help] [-c^|--compiler ^<clang^|msvc^>] [-a^|--arch ^<x86^|x64^|arm64^>]
    echo         [-p^|--vcpkg path/to/vcpkg/root] [-v^|--version X.Y.Z]
    echo         [--cppwinrt ^<version^>] [--fast]
    echo.
    echo ARGUMENTS
    echo     -c^|--compiler       Controls the compiler used, either 'clang' (the default) or 'msvc'
    echo     -a^|--arch           Controls the architecture used, either 'x86', 'x64' or 'arm64'. The default is to use
    echo                         the architecture of the Visual Studio command window that is used to run this script.
    echo     -p^|--vcpkg          Specifies the path to the root of your local vcpkg clone. If this value is not
    echo                         specified, then several attempts will be made to try and deduce it. The first attempt
    echo                         will be to check for the presence of the %%VCPKG_ROOT%% environment variable. If that
    echo                         variable does not exist, the 'where' command will be used to try and locate the
    echo                         vcpkg.exe executable. If that check fails, the path '\vcpkg' will be used to try and
    echo                         locate the vcpkg clone. If all those checks fail, initialization will fail.
    echo     -v^|--version        Specifies the version of the NuGet package produced. Primarily only used by the CI
    echo                         build and is typically not necessary when building locally
    echo     --cppwinrt          Manually specifies the version of C++/WinRT to use for generating headers
    echo     --fast              Used to (slightly) reduce compile times and build output size. This is primarily
    echo                         used by the CI build machines where resources are more constrained. This switch is
    echo                         temporary and will be removed once https://github.com/microsoft/wil/issues/9 is fixed
    goto :eof

:init
    :: Initialize values as empty so that we can identify if we are using defaults later for error purposes
    set COMPILER=
    set CMAKE_ARGS=
    set BUILD_ARCH=
    set VERSION=
    set VCPKG_ROOT_PATH=
    set CPPWINRT_VERSION=
    set FAST_BUILD=0

:parse
    if /I "%~1"=="" goto :execute

    if /I "%~1"=="--help" call :usage & goto :eof

    set COMPILER_SET=0
    if /I "%~1"=="-c" set COMPILER_SET=1
    if /I "%~1"=="--compiler" set COMPILER_SET=1
    if %COMPILER_SET%==1 (
        if "%COMPILER%" NEQ "" echo ERROR: Compiler already specified & call :usage & exit /B 1

        if /I "%~2"=="clang" set COMPILER=clang
        if /I "%~2"=="msvc" set COMPILER=msvc
        if "!COMPILER!"=="" echo ERROR: Unrecognized/missing compiler %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set VCPKG_ROOT_SET=0
    if /I "%~1"=="-p" set VCPKG_ROOT_SET=1
    if /I "%~1"=="--vcpkg" set VCPKG_ROOT_SET=1
    if %VCPKG_ROOT_SET%==1 (
        if "%VCPKG_ROOT_PATH%" NEQ "" echo ERROR: vcpkg root path already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: Path to vcpkg root missing & call :usage & exit /B 1

        set VCPKG_ROOT_PATH=%~2

        shift
        shift
        goto :parse
    )

    set VERSION_SET=0
    if /I "%~1"=="-v" set VERSION_SET=1
    if /I "%~1"=="--version" set VERSION_SET=1
    if %VERSION_SET%==1 (
        if "%VERSION%" NEQ "" echo ERROR: Version already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: Version string missing & call :usage & exit /B 1

        set VERSION=%~2

        shift
        shift
        goto :parse
    )

    set BUILD_ARCH_SET=0
    if /I "%~1"=="-a" set BUILD_ARCH_SET=1
    if /I "%~1"=="--arch" set BUILD_ARCH_SET=1
    if %BUILD_ARCH_SET%==1 (
        if "%BUILD_ARCH%" NEQ "" echo ERROR: Architecture already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: Architecture string missing & call :usage & exit /B 1

        if /I "%~2"=="x86" set BUILD_ARCH=x86
        if /I "%~2"=="x64" set BUILD_ARCH=x64
        if /I "%~2"=="arm64" set BUILD_ARCH=arm64
        if "!BUILD_ARCH!"=="" echo ERROR: Unrecognized architecture %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    if /I "%~1"=="--cppwinrt" (
        if "%CPPWINRT_VERSION%" NEQ "" echo ERROR: C++/WinRT version already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: C++/WinRT version string missing & call :usage & exit /B 1

        set CPPWINRT_VERSION=%~2

        shift
        shift
        goto :parse
    )

    if /I "%~1"=="--fast" (
        if %FAST_BUILD% NEQ 0 echo ERROR: Fast build already specified
        set FAST_BUILD=1
        shift
        goto :parse
    )

    echo ERROR: Unrecognized argument %~1
    call :usage
    exit /B 1

:execute
    if "%COMPILER%"=="" set COMPILER=clang

    if "%VCPKG_ROOT_PATH%"=="" (
        :: First check for %VCPKG_ROOT% variable
        if defined VCPKG_ROOT (
            set VCPKG_ROOT_PATH=%VCPKG_ROOT%
        ) else (
            :: Next check the PATH for vcpkg.exe
            for %%i in (vcpkg.exe) do set VCPKG_ROOT_PATH=%%~dp$PATH:i

            if "!VCPKG_ROOT_PATH!"=="" (
                :: Finally, check the root of the drive for a clone of the name 'vcpkg'
                if exist \vcpkg\vcpkg.exe (
                    for %%i in (%cd%) do set VCPKG_ROOT_PATH=%%~di\vcpkg
                )
            )
        )
    )
    if "%VCPKG_ROOT_PATH%"=="" (
        echo ERROR: Unable to locate the root path of your local vcpkg installation.
        :: TODO: Better messaging
        exit /B 1
    )

    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_SYSTEM_VERSION=10.0

    if "%VERSION%" NEQ "" set CMAKE_ARGS=%CMAKE_ARGS% -DWIL_BUILD_VERSION=%VERSION%

    if "%CPPWINRT_VERSION%" NEQ "" set CMAKE_ARGS=%CMAKE_ARGS% -DCPPWINRT_VERSION=%CPPWINRT_VERSION%

    if %FAST_BUILD%==1 set CMAKE_ARGS=%CMAKE_ARGS% -DFAST_BUILD=ON

    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    :: Figure out the platform
    if "%Platform%"=="" echo ERROR: The init.cmd script must be run from a Visual Studio command window & exit /B 1
    if "%Platform%"=="x86" (
        set BITNESS=32
        if %COMPILER%==clang set CFLAGS=-m32 & set CXXFLAGS=-m32
    )
    if "%Platform%"=="x64" set BITNESS=64
    if "%BITNESS%"=="" echo ERROR: Unrecognized/unsupported platform %Platform% & exit /B 1

    if "%BUILD_ARCH%"=="" set BUILD_ARCH=%Platform%

    :: Finalize the CMAKE_ARGS
    set CMAKE_ARGS=-S %SOURCE_DIR% --preset %BUILD_ARCH%-%COMPILER% %CMAKE_ARGS%

    :: Run CMake
    echo Using compiler....... %COMPILER%
    echo Using architecture... %Platform%
    echo Full cmake arguments. %CMAKE_ARGS%
    echo.
    cmake %CMAKE_ARGS%

    goto :eof
