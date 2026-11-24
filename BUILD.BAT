@echo off
rem Build QuadDesk with Open Watcom C 2.0 (DOS or Windows).
rem Requires the WATCOM environment variable and %WATCOM%\BINW in the PATH.

if "%WATCOM%"=="" goto nowatcom
set INCLUDE=%WATCOM%\H;%WATCOM%\H\WIN
if not exist BUILD\NUL md BUILD
cd BUILD

echo Building QDHOOK.DLL
wcc -q -bt=windows -ml -zw -ox -w4 -i=..\SRC -bd -zu -fo=qdhook.obj ..\SRC\QDHOOK.C
if errorlevel 1 goto error
wlink op q system windows_dll file qdhook.obj name qdhook.dll
if errorlevel 1 goto error
wlib -q -n qdhook.lib +qdhook.dll
if errorlevel 1 goto error

echo Building QUADDESK.EXE
wcc -q -bt=windows -ml -zw -ox -w4 -i=..\SRC -fo=quaddesk.obj ..\SRC\QUADDESK.C
if errorlevel 1 goto error
wlink op q system windows file quaddesk.obj library qdhook.lib name quaddesk.exe option stack=8k option heapsize=4k
if errorlevel 1 goto error

cd ..
echo Done: BUILD\QUADDESK.EXE and BUILD\QDHOOK.DLL
goto end

:nowatcom
echo Set the WATCOM environment variable first.
goto end

:error
cd ..
echo *** Build failed ***

:end
