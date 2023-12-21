@echo off
:: NOTE: No 'setlocal' since we're "returning" the definition of "CLANG_FORMAT" to the caller

:: Clang format's behavior has changed over time, meaning that different machines with different versions of LLVM
:: installed may get different formatting behavior. In an attempt to ensure consistency, we use the clang-format.exe
:: that ships with Visual Studio. There may still be issues if two different machines have different versions of Visual
:: Studio installed, however this will hopefully improve things
set CLANG_FORMAT=%VCINSTALLDIR%\Tools\Llvm\bin\clang-format.exe
if not exist "%CLANG_FORMAT%" (
    set CLANG_FORMAT=
    echo ERROR: clang-format.exe not found at %%VCINSTALLDIR%%\Tools\Llvm\bin\clang-format.exe
    echo ERROR: Ensure that this script is being run from a Visual Studio command prompt
    exit /B 1
)
