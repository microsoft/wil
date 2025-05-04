@echo off
setlocal
setlocal EnableDelayedExpansion

:: NOTE: By default, the target architecture is picked up from the %Platform% environment variable.
::       You can overreide it by passing the -a or --arch argument to this script. See init.cmd for details.
set COMPILERS=clang msvc

for %%c in (%COMPILERS%) do (
    echo INFO: Generating CMake cache for %%c
    call %~dp0\init.cmd -c %%c %*
    if !ERRORLEVEL! NEQ 0 ( goto :eof )
)
