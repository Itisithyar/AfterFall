@echo off
REM REAL GAME verification (headless, no GUI required). Destination-local
REM runner: uses RealGame.uproject and this tree only. Proves nothing about
REM the source project; source logs must never be cited for this tree.
REM
REM 1. Builds AfterfallEditor for RealGame (all ported game C++).
REM 2. Runs the Afterfall.* automation suite headless (UnrealEditor-Cmd,
REM    editor context) and reports TEST COMPLETE exit code.
REM 3. Runs the afRoomSelfTest fixture headless: dedicated server
REM    (UnrealEditor.exe -server) on /Engine/Maps/Entry (no committed
REM    Content exists here; the fixture is map-agnostic and spawns its own).
REM Logs go to Saved\RoomTest\ with a timestamped name.
REM
REM NOTE: flags inside this file use single slashes (/t, /c:, /PID, /F).
REM Double slashes are only for commands typed through the Git-Bash tool
REM layer, never inside .bat files.
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
set STAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
set STAMP=%STAMP: =0%
set OUT=%OUTDIR%\realgame_%STAMP%.out

echo [1/4] Building AfterfallEditor (Real Game)...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)

echo [2/4] Running Afterfall automation suite...
"%UE%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL% >> "%OUT%"
if ERRORLEVEL 1 (echo AUTOMATION TESTS FAILED & exit /b 1)

echo [3/4] Running afRoomSelfTest (dedicated server, Entry map)...
echo START %TIME% >> "%OUT%"
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds="afRoomSelfTest" -log -NoVerifyGC -stdout -FullStdOutLogOutput >> "%OUT%" 2>&1
set ROOMEXIT=%ERRORLEVEL%
echo ROOMTEST_EXIT:%ROOMEXIT% >> "%OUT%"
findstr /c:"ROOMTEST SUMMARY" "%OUT%"
if NOT "%ROOMEXIT%"=="0" (echo ROOMTEST FAIL ^(exit %ROOMEXIT%^) & exit /b 1)
findstr /c:"ROOMTEST PASS" "%OUT%" >nul
if ERRORLEVEL 1 (echo ROOMTEST FAIL ^(log^) & exit /b 1)
echo ROOMTEST PASS

REM Stage 4 runs the PRODUCTION damage path in a real game world: the pawn's
REM melee/hitscan server RPCs damage a runtime-spawned structure, then collision,
REM delta export and reload restoration are asserted. It needs a local player,
REM so it runs -game (not -server); the fixture exits 0/1 itself.
echo [4/4] Running afPipelineSelfTest (game world, production damage path)...
set PIPE_OUT=%OUTDIR%\pipeline_%STAMP%.out
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -game -unattended -nopause -nosplash -nullrhi -nosound -afPipelineSelfTest -log -NoVerifyGC -stdout -FullStdOutLogOutput >> "%PIPE_OUT%" 2>&1
set PIPEEXIT=%ERRORLEVEL%
findstr /c:"PIPELINE SUMMARY" "%PIPE_OUT%"
if NOT "%PIPEEXIT%"=="0" (echo PIPELINE FAIL ^(exit %PIPEEXIT%^) & exit /b 1)
findstr /c:"PIPELINE PASS" "%PIPE_OUT%" >nul
if ERRORLEVEL 1 (echo PIPELINE FAIL ^(log^) & exit /b 1)
echo PIPELINE PASS
echo Report: %OUT%
echo Pipeline report: %PIPE_OUT%
endlocal
exit /b 0
