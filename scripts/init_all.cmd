@echo off

:: NOTE: Architecture is picked up from the command window, so we can't control that here :(

:: NOTE: There's currently a bug where Clang and/or the linker chokes when trying to compile the tests for 32-bit debug, so skip for now
if "%Platform%"=="x86" goto :skip_clang_x86_debug
call %~dp0\init.cmd clang ninja debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:skip_clang_x86_debug
:: call %~dp0\init.cmd clang ninja release
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )
call %~dp0\init.cmd clang ninja relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:: call %~dp0\init.cmd clang ninja minsizerel
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )

call %~dp0\init.cmd msvc ninja debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:: call %~dp0\init.cmd msvc ninja release
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )
call %~dp0\init.cmd msvc ninja relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:: call %~dp0\init.cmd msvc ninja minsizerel
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )
