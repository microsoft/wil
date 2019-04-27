@echo off

:: NOTE: Architecture is picked up from the command window, so we can't control that here :(

:: NOTE: There's currently a bug where Clang and/or the linker chokes when trying to compile the tests for 32-bit debug, so skip for now
if "%Platform%"=="x86" goto :skip_clang_x86_debug
call %~dp0\init.cmd -c clang -g ninja -b debug %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:skip_clang_x86_debug
:: call %~dp0\init.cmd -c clang -g ninja -b release %*
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )
call %~dp0\init.cmd -c clang -g ninja -b relwithdebinfo %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:: call %~dp0\init.cmd -c clang -g ninja -b minsizerel %*
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )

call %~dp0\init.cmd -c msvc -g ninja -b debug %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:: call %~dp0\init.cmd -c msvc -g ninja -b release %*
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )
call %~dp0\init.cmd -c msvc -g ninja -b relwithdebinfo %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
:: call %~dp0\init.cmd -c msvc -g ninja -b minsizerel %*
:: if %ERRORLEVEL% NEQ 0 ( goto :eof )
