@echo off
setlocal
setlocal EnableDelayedExpansion

set ROOT_DIR=%~dp0\..

set DIRS=include/wil tests
set EXTS=.cpp .h

call "%~dp0/find-clang-format.cmd"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

for %%d in (%DIRS%) do call :format_files %ROOT_DIR%\%%d
goto :eof

:format_files
    :: Format all desired files
    for %%e in (%EXTS%) do (
        for %%f in (%1\*%%e) do call :run_clang_format %%f
    )
    goto :eof

:run_clang_format
    "%CLANG_FORMAT%" -style=file -i %1
    goto :eof
