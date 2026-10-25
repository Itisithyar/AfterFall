@echo off
REM AFTERFALL UE runtime verification (headless, no GUI required).
REM
REM 1. Builds AfterfallEditor (all game C++ incl. DestructionRoomSubsystem).
REM 2. Runs the Afterfall.* automation suite headless (UnrealEditor-Cmd,
REM    editor context) and reports TEST COMPLETE exit code.
REM 3. Runs the afRoomSelfTest room fixture headless: dedicated server
REM    (UnrealEditor.exe -server) on the minimal NON-partitioned engine map
REM    /Engine/Maps/Entry. The template map Lvl_FirstPerson is World-
REM    Partitioned and trips an engine GEditor assert in game-in-editor
REM    (-game/-server) runs, so it cannot be used for this path. The room
REM    fixture is map-agnostic and spawns everything itself.
REM
REM NOTE: the standalone game binary (Afterfall.exe) cannot run uncooked
REM content (no local cook + unreachable Zen store), so game-in-editor and
REM dedicated-server-in-editor are the supported runtime paths here.
REM Logs go to Saved\RoomTest\ with a timestamped name.
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
set STAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
set STAMP=%STAMP: =0%
set OUT=%OUTDIR%\ue_runtime_%STAMP%.out

echo [1/3] Building AfterfallEditor...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)

echo [2/3] Running Afterfall automation suite...
"%UE%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL% >> "%OUT%"
if ERRORLEVEL 1 (echo AUTOMATION TESTS FAILED & exit /b 1)

echo [3/3] Running afRoomSelfTest (dedicated server, Entry map)...
echo START %TIME% > "%OUT%"
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds="afRoomSelfTest" -log -NoVerifyGC -stdout -FullStdOutLogOutput >> "%OUT%" 2>&1
echo ROOMTEST_EXIT:%ERRORLEVEL% >> "%OUT%"
echo Report: %OUT%
endlocal
