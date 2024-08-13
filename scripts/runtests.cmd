@echo off
setlocal
setlocal EnableDelayedExpansion

set TEST_ARGS=%*

:: For some reason, '__asan_default_options' seems to have no effect under some unknown circumstances (despite the
:: function being called), so set the environment variable as a workaround. This ensures that we get the correct
:: behavior at least when this script is being used, which should cover most developer scenarios as well as the CI
set ASAN_OPTIONS=allocator_may_return_null=1:new_delete_type_mismatch=0

set BUILD_ROOT=%~dp0\..\build

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel

:: The asan binaries are architecture specific, so we unfortunately must limit the tests we run by the architecture of
:: the command window.
if "%Platform%"=="x64" (
    set ARCHITECTURES=64
) else (
    set ARCHITECTURES=32
)

for %%c in (%COMPILERS%) do (
    for %%a in (%ARCHITECTURES%) do (
        for %%b in (%BUILD_TYPES%) do (
            call :execute_tests %%c%%a%%b
            if !ERRORLEVEL! NEQ 0 ( goto :eof )
        )
    )
)

goto :eof

:execute_tests
set BUILD_DIR=%BUILD_ROOT%\%1
if not exist %BUILD_DIR% ( goto :eof )

pushd %BUILD_DIR%
echo Running tests from %CD%
call :execute_test app witest.app.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test cpplatest witest.cpplatest.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test cppwinrt-com-server witest.cppwinrt-com-server.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test cppwinrt-com-server-custom-module-lock witest.cppwinrt-com-server-custom-module-lock.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test noexcept witest.noexcept.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test normal witest.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test sanitize-address witest.asan.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test sanitize-undefined-behavior witest.ubsan.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test win7 witest.win7.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
:: Fall through

:execute_tests_done
set EXIT_CODE=%ERRORLEVEL%
popd
exit /B %EXIT_CODE%

:execute_test
if not exist tests\%1\%2 ( goto :eof )
echo Running %1 tests...
tests\%1\%2 %TEST_ARGS%
goto :eof
