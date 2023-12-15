@echo off
setlocal
setlocal EnableDelayedExpansion

set ROOT_DIR=%~dp0\..

set DIRS=include tests
set EXTS=.cpp .h

:: Clang format's behavior has changed over time, meaning that different machines with different versions of LLVM
:: installed may get different formatting behavior. In an attempt to ensure consistency, we use the clang-format.exe
:: that ships with Visual Studio. There may still be issues if two different machines have different versions of Visual
:: Studio installed, however this will hopefully improve things
set CLANG_FORMAT=%VCINSTALLDIR%\Tools\Llvm\bin\clang-format.exe
if not exist "%CLANG_FORMAT%" (
    echo ERROR: clang-format.exe not found at %%VCINSTALLDIR%%\Tools\Llvm\bin\clang-format.exe
    echo ERROR: Ensure that this script is being run from a Visual Studio command prompt
    exit /B 1
)

for %%d in (%DIRS%) do call :recursive_format %ROOT_DIR%\%%d
goto :eof

:recursive_format
    :: First do files
    for %%e in (%EXTS%) do (
        for %%f in (%1\*%%e) do call :run_clang_format %%f
    )
    :: Now do subdirectories
    for /d %%d in (%1\*) do call :recursive_format %%d
    goto :eof

:run_clang_format
    "%CLANG_FORMAT%" -style=file -i %1
    goto :eof
