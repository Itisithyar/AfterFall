@echo off
REM Menu+Lab verification: afRoomSelfTest on Entry (dedicated server).
setlocal
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUT=%~dp0..\Saved\RoomTest\menulab_room.out
"%UE%\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound -ExecCmds="afRoomSelfTest" -log -NoVerifyGC -stdout -FullStdOutLogOutput > "%OUT%" 2>&1
echo ROOMTEST_EXIT:%ERRORLEVEL%
endlocal
