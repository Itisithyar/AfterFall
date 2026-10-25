@echo off
REM Temporary: quadratic fracture + radial interaction automation run.
REM Headless (nullrhi), house pattern from run_ue_runtime_tests.bat.
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
"%UE%\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -TestExit="Automation Test Queue Empty"
echo AUTOMATION_EXIT:%ERRORLEVEL%
endlocal