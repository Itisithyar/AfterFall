@echo off
setlocal
set UEDIR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%UEDIR%\UnrealEditor.exe' -ArgumentList '\"%PROJ%\"', '/Engine/Maps/Entry', '-server', '-unattended', '-nopause', '-nosplash', '-nosound', '-afRoomServer', '-afRoomWaitClients=1', '-log', '-NoVerifyGC', '-stdout', '-FullStdOutLogOutput' -PassThru -RedirectStandardOutput '%OUTDIR%\live1_server.out' -RedirectStandardError '%OUTDIR%\live1_server.err'; $p.Id | Out-File -FilePath '%OUTDIR%\live1_server.pid' -NoNewline -Encoding ascii"
endlocal
