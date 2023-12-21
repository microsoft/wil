@echo off
setlocal
setlocal EnableDelayedExpansion

call "%~dp0/find-clang-format.cmd"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
