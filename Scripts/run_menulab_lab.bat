@echo off
REM Menu+Lab verification: afPhysicsLab on Entry (dedicated server).
REM Lab never exits by itself; the caller kills the editor after ~100s
REM and greps PHYLAB lines from the output.
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUT=%~dp0..\Saved\RoomTest\menulab_lab3.out
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds=afPhysicsLog;afPhysicsLab -log -NoVerifyGC -stdout -FullStdOutLogOutput > "%OUT%" 2>&1
echo LAB_EXIT:%ERRORLEVEL%
endlocal
