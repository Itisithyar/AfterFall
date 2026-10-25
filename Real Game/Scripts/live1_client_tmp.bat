@echo off
set UEDIR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
del "%OUTDIR%\live1_client.out" 2>nul
"%UEDIR%\UnrealEditor.exe" "%PROJ%" 127.0.0.1 -game -nullrhi -unattended -nopause -nosplash -nosound -afRoomLiveClient -log -NoVerifyGC -stdout -FullStdOutLogOutput > "%OUTDIR%\live1_client.out" 2>&1
echo CLIENT_EXIT:%ERRORLEVEL%
