@echo off
setlocal
setlocal EnableDelayedExpansion

set THIS_DIR=%~dp0\

:repeat
REM If you're running this test, you probably don't want it constantly clearing out your clipboard...
call %THIS_DIR%\runtests.cmd ~UniqueCloseClipboardCall %*
if %ERRORLEVEL%==0 goto :repeat
