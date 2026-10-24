@echo off
REM AFTERFALL headless game test — runs under cmd.exe so /Game/ args are NOT rewritten.
REM Usage: Scripts\launch_headless_gametest.cmd [Map]
setlocal
set MAP=%1
if "%MAP%"=="" set MAP=/Game/FirstPerson/Lvl_FirstPerson
cd /d "%~dp0.."
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\RealGame.uproject" %MAP% -game -unattended -nopause -nullrhi -log
endlocal
