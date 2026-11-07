@echo off
setlocal
setlocal EnableDelayedExpansion

set PROJECT_ROOT=%~dp0\..

set BRANCH=%1
if "%BRANCH%"=="" (
    echo ERROR: Missing commit/branch argument. If this is a fork of the microsoft/wil repo, you likely want to specify
    echo        'upstream/master'. If this is not a fork, you likely want to specify 'origin/master'. Examples:
    echo.
    echo            format-changes.cmd origin/master
    echo            format-changes.cmd upstream/master
    exit /b 1
)

call "%~dp0/find-clang-format.cmd"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

pushd %PROJECT_ROOT% > NUL
where git-clang-format.bat > NUL 2>&1
if errorlevel 1 (
    git clang-format %BRANCH% --binary "%CLANG_FORMAT%" --style file -- include/wil/*.h tests/*.h tests/*.cpp
) else (
    call git-clang-format.bat %BRANCH% --binary "%CLANG_FORMAT%" --style file -- include/wil/*.h tests/*.h tests/*.cpp
)
set EXIT_CODE=%ERRORLEVEL%
popd > NUL
exit /B %EXIT_CODE%
