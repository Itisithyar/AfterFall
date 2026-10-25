@echo off
REM AFTERFALL world fixtures: voxel terrain + destructible buildings + mechanics.
REM
REM Both run headless on the dedicated-server path with the engine Entry map, so
REM nothing in Content/ has to be authored. Exit code is non-zero if either
REM fixture fails.
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set LOGDIR=%~dp0..\Saved\Logs
mkdir "%LOGDIR%" 2>nul

echo [1/3] Building AfterfallEditor...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)

echo [2/3] WORLDGEN fixture (1 km plan, 16 chunks, 8 building shells)...
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause ^
  -nosplash -nosound -nullrhi -afWorldGenSelfTest -NoVerifyGC -stdout ^
  -FullStdOutLogOutput -log -abslog="%LOGDIR%\WorldGenTest.log"
if ERRORLEVEL 1 (echo WORLDGEN FAILED & exit /b 1)

echo [3/3] MECHTEST fixture (playground, destruction, grapple)...
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause ^
  -nosplash -nosound -nullrhi -afMechSelfTest -NoVerifyGC -stdout ^
  -FullStdOutLogOutput -log -abslog="%LOGDIR%\MechTest.log"
if ERRORLEVEL 1 (echo MECHTEST FAILED & exit /b 1)

echo Both fixtures passed. Logs: %LOGDIR%\WorldGenTest.log, %LOGDIR%\MechTest.log
endlocal
