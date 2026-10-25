@echo off
REM aftestrun - consolidated acceptance entry point for Real Game
setlocal
set ROOT=%~dp0..
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%ROOT%\RealGame.uproject
set OUTDIR=%ROOT%\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
set STAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
set STAMP=%STAMP: =0%
set OUT=%OUTDIR%\aftestrun_%STAMP%.out
echo AFTESRUN start %DATE% %TIME% > "%OUT%"
echo [1/3] Build...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
echo BUILD_EXIT:%ERRORLEVEL% >> "%OUT%"
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)
echo [2/3] Automation...
"%UE%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL% >> "%OUT%"
echo [3/3] Room self-test...
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds="afRoomSelfTest" -log -NoVerifyGC -stdout -FullStdOutLogOutput >> "%OUT%" 2>&1
echo ROOMTEST_EXIT:%ERRORLEVEL% >> "%OUT%"
findstr /c:"ROOMTEST PASS" "%OUT%" >nul
if ERRORLEVEL 1 (echo ROOMTEST FAIL & exit /b 1)
echo ROOMTEST PASS
echo Report: %OUT%
endlocal
exit /b 0
