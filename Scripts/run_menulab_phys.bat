@echo off
REM Menu+Lab verification: afPhysicsSelfTest (dedicated server, Entry map).
REM The fixture exits the process itself: 0 = all checks pass.
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUT=%~dp0..\Saved\RoomTest\menulab_phys.out
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds="afPhysicsSelfTest" -log -NoVerifyGC -stdout -FullStdOutLogOutput > "%OUT%" 2>&1
echo PHYSTEST_EXIT:%ERRORLEVEL%
endlocal
