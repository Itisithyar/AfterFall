@echo off
REM AFTERFALL voxel replication check (headless, real network, 2 processes).
REM
REM 1. Dedicated server on /Engine/Maps/Entry with -afVoxelServer: spawns and
REM    initialises the single chunk, applies THREE authoritative edits while no
REM    client is connected, logs VOXELSERVER READY, and stays up.
REM 2. Real client (127.0.0.1) with -afVoxelClientVerify: joins LATE, asserts the
REM    edited state converged via the initial snapshot, checks client mesh +
REM    collision, sends an invalid edit (must be ignored), then a valid edit
REM    (must round trip), logs VOXELCLIENT SUMMARY and exits.
REM
REM NOTE: flags inside this .bat use single slashes; double slashes are only for
REM commands typed through the Git-Bash tool layer.
setlocal
set UEDIR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\VoxelTest
mkdir "%OUTDIR%" 2>nul
del "%OUTDIR%\voxel_server.out" 2>nul
del "%OUTDIR%\voxel_client.out" 2>nul
del "%OUTDIR%\voxel_server.pid" 2>nul

if /i "%1"=="skipbuild" goto skipbuild
echo [1/4] Building AfterfallEditor (Real Game)...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)
:skipbuild

echo [2/4] Starting dedicated voxel server...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%UEDIR%\UnrealEditor.exe' -ArgumentList '\"%PROJ%\"', '/Engine/Maps/Entry', '-server', '-unattended', '-nopause', '-nosplash', '-nosound', '-afVoxelServer', '-log', '-NoVerifyGC', '-stdout', '-FullStdOutLogOutput' -PassThru -RedirectStandardOutput '%OUTDIR%\voxel_server.out' -RedirectStandardError '%OUTDIR%\voxel_server.err'; $p.Id | Out-File -FilePath '%OUTDIR%\voxel_server.pid' -NoNewline -Encoding ascii"
set /a TRIES=0
:serverwait
ping -n 4 127.0.0.1 >nul
findstr /c:"VOXELSERVER READY" "%OUTDIR%\voxel_server.out" >nul && goto serverready
set /a TRIES+=1
if %TRIES% GEQ 80 (echo SERVER TIMEOUT: no VOXELSERVER READY & goto killfail)
goto serverwait

:serverready
echo [3/4] Server ready. Starting late-joining client...
"%UEDIR%\UnrealEditor.exe" "%PROJ%" 127.0.0.1 -game -nullrhi -unattended -nopause -nosplash -nosound -afVoxelClientVerify -log -NoVerifyGC -stdout -FullStdOutLogOutput > "%OUTDIR%\voxel_client.out" 2>&1
set CLIENTEXIT=%ERRORLEVEL%

echo [4/4] Client done (%CLIENTEXIT%). Stopping server...
set /p SRVPID=<"%OUTDIR%\voxel_server.pid"
taskkill /PID %SRVPID% /T /F >nul 2>nul
echo --- server ---
findstr /c:"VOXELSERVER READY" "%OUTDIR%\voxel_server.out"
echo --- client ---
findstr /c:"VOXELCLIENT PASS" "%OUTDIR%\voxel_client.out"
findstr /c:"VOXELCLIENT FAIL" "%OUTDIR%\voxel_client.out"
findstr /c:"VOXELCLIENT SUMMARY" "%OUTDIR%\voxel_client.out"
findstr /c:"VOXELCLIENT SUMMARY" "%OUTDIR%\voxel_client.out" | findstr /c:"VOXELCLIENT PASS" >nul && echo VOXEL REPLICATION PASS || echo VOXEL REPLICATION FAIL
endlocal
exit /b 0

:killfail
set /p SRVPID=<"%OUTDIR%\voxel_server.pid"
taskkill /PID %SRVPID% /T /F >nul 2>nul
echo VOXEL REPLICATION BLOCKED
endlocal
exit /b 1
