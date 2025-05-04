@echo off
setlocal
setlocal EnableDelayedExpansion

set TEST_ARGS=%*

:: For some reason, '__asan_default_options' seems to have no effect under some unknown circumstances (despite the
:: function being called), so set the environment variable as a workaround. This ensures that we get the correct
:: behavior at least when this script is being used, which should cover most developer scenarios as well as the CI
set ASAN_OPTIONS=allocator_may_return_null=1:new_delete_type_mismatch=0

set BUILD_ROOT=%~dp0\..\build

for /f %%i in ('dir /s /b %BUILD_ROOT%\witest*.exe') do (
    set TEST_ERROR=
    call :runtest %%i
    if !ERRORLEVEL! NEQ 0 (
        set TEST_ERROR=1
        echo ERROR: Test %1 failed with error code !ERRORLEVEL!
        exit /b !ERRORLEVEL!
    )
)
goto :eof

:runtest
set TESTDIR=%~dp1
pushd %TESTDIR%
echo Running %1
call %1 %TEST_ARGS%
set TEST_ERROR=%ERRORLEVEL%
popd
exit /B %TEST_ERROR%
