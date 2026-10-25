@echo off
REM Temporary runner (BEAM): BoxBuilding automation suite headless.
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject
"%UE%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall.BoxBuilding; Quit" -log -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL%
