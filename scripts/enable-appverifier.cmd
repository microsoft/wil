@echo off
setlocal
setlocal EnableDelayedExpansion

REM NOTE: AppVerifier & ASan don't mix well, so leaving it out
set EXES=witest.app witest.cpplatest witest.cppwinrt-notifiable-server-lock witest.noexcept witest witest.win7
set LAYERS=Exceptions Handles Heaps Locks Memory SRWLock Threadpool TLS

for %%e in (%EXES%) do (
    appverif /enable %LAYERS% /for %%e.exe
)
