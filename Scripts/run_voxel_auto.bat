@echo off
REM Voxel verification: Afterfall.* automation suite headless (own log file).
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set LOG=%~dp0..\Saved\Logs\VoxelAuto.log
"%UE%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -abslog="%LOG%" -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL%
endlocal
