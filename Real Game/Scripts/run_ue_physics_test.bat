@echo off
REM AFTERFALL physics acceptance (headless, no GUI required).
REM
REM 1. Builds AfterfallEditor (all game C++ incl. PhysicsValidationSubsystem
REM    and the repaired character input scheme).
REM 2. Runs the Afterfall.* automation suite headless (UnrealEditor-Cmd,
REM    editor context) and reports TEST COMPLETE exit code.
REM 3. Runs the afPhysicsSelfTest fixture headless: dedicated server
REM    (UnrealEditor.exe -server) on the minimal NON-partitioned engine map
REM    /Engine/Maps/Entry. The fixture spawns its own floor/test-body rig
REM    and checks free-fall vs d=1/2*g*t^2, impulse J=m*dv, contact/settle,
REM    friction, the real pickup/throw/drop path, and control defaults.
REM    The process exits 0 on PHYTEST PASS, 1 on PHYTEST FAIL (plus a
REM    180s in-fixture watchdog).
REM
REM NOTE: flags inside this file use single slashes (/t, /c:, /PID, /F).
REM Double slashes are only for commands typed through the Git-Bash tool
REM layer, never inside .bat files.
setlocal
set UEDIR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
set STAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
set STAMP=%STAMP: =0%
set OUT=%OUTDIR%\physics_%STAMP%.out
del "%OUTDIR%\physics_latest.out" 2>nul

echo [1/3] Building AfterfallEditor...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)

echo [2/3] Running Afterfall automation suite...
"%UEDIR%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL% > "%OUT%"
if ERRORLEVEL 1 (echo AUTOMATION TESTS FAILED & exit /b 1)

echo [3/3] Running afPhysicsSelfTest (dedicated server, Entry map)...
echo START %TIME% >> "%OUT%"
"%UEDIR%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds="afPhysicsSelfTest" -log -NoVerifyGC -stdout -FullStdOutLogOutput >> "%OUT%" 2>&1
set PHYEXIT=%ERRORLEVEL%
echo PHYTEST_EXIT:%PHYEXIT% >> "%OUT%"
copy "%OUT%" "%OUTDIR%\physics_latest.out" >nul
findstr /c:"PHYTEST SUMMARY" "%OUT%"
if NOT "%PHYEXIT%"=="0" (echo PHYSICS FAIL ^(exit %PHYEXIT%^) & exit /b 1)
findstr /c:"PHYTEST PASS" "%OUT%" >nul
if ERRORLEVEL 1 (echo PHYSICS FAIL ^(log^) & exit /b 1)
echo PHYSICS PASS
endlocal
exit /b 0
