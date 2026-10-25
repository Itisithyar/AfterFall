@echo off
REM AFTERFALL room replication check (headless, no GUI required).
REM
REM 1. Builds AfterfallEditor.
REM 2. Starts a dedicated server (UnrealEditor.exe -server) on the minimal
REM    non-partitioned engine map /Engine/Maps/Entry with -afRoomServer:
REM    builds the room, applies melee+hitscan+rotated impacts, logs
REM    ROOMSERVER READY, and stays up. Stdout goes to repl_server.out.
REM 3. Starts a real client (UnrealEditor.exe 127.0.0.1 -game -nullrhi) with
REM    -afRoomClientVerify: joins AFTER the impacts (late-join), asserts
REM    replicated detached state, visuals, collision, and that client-side
REM    commits are rejected, logs ROOMCLIENT SUMMARY, and quits. Stdout
REM    goes to repl_client.out.
REM 4. Kills the server by PID and reports PASS/FAIL from both logs.
REM
REM Why Entry, not Lvl_FirstPerson: the template map is World-Partitioned
REM and trips an engine GEditor assert in game-in-editor (-game/-server)
REM runs. The room fixture is map-agnostic. The standalone game binary
REM cannot run uncooked content here (no local cook, Zen store unreachable).
REM
REM NOTE: flags inside this file use single slashes (/t, /c:, /PID, /F).
REM Double slashes are only for commands typed through the Git-Bash tool
REM layer, never inside .bat files.
setlocal
set UEDIR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
del "%OUTDIR%\repl_server.out" 2>nul
del "%OUTDIR%\repl_server.err" 2>nul
del "%OUTDIR%\repl_client.out" 2>nul
del "%OUTDIR%\repl_server.pid" 2>nul

echo [1/4] Building AfterfallEditor...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)

echo [2/4] Starting dedicated server...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%UEDIR%\UnrealEditor.exe' -ArgumentList '\"%PROJ%\"', '/Engine/Maps/Entry', '-server', '-unattended', '-nopause', '-nosplash', '-nosound', '-afRoomServer', '-log', '-NoVerifyGC', '-stdout', '-FullStdOutLogOutput' -PassThru -RedirectStandardOutput '%OUTDIR%\repl_server.out' -RedirectStandardError '%OUTDIR%\repl_server.err'; $p.Id | Out-File -FilePath '%OUTDIR%\repl_server.pid' -NoNewline -Encoding ascii"
set /a TRIES=0
:serverwait
ping -n 4 127.0.0.1 >nul
findstr /c:"ROOMSERVER READY" "%OUTDIR%\repl_server.out" >nul && goto serverready
set /a TRIES+=1
if %TRIES% GEQ 80 (echo SERVER TIMEOUT: no ROOMSERVER READY & goto killserverfail)
goto serverwait
:serverready
echo [3/4] Server ready. Starting client...
"%UEDIR%\UnrealEditor.exe" "%PROJ%" 127.0.0.1 -game -nullrhi -unattended -nopause -nosplash -nosound -afRoomClientVerify -log -NoVerifyGC -stdout -FullStdOutLogOutput > "%OUTDIR%\repl_client.out" 2>&1
echo [4/4] Client done. Stopping server...
:killserver
set /p SRVPID=<"%OUTDIR%\repl_server.pid"
taskkill /PID %SRVPID% /F
findstr /c:"ROOMSERVER READY" "%OUTDIR%\repl_server.out"
findstr /c:"ROOMCLIENT SUMMARY" "%OUTDIR%\repl_client.out"
findstr /c:"ROOMCLIENT PASS" "%OUTDIR%\repl_client.out" && echo REPLICATION PASS || echo REPLICATION FAIL
endlocal
exit /b 0
:killserverfail
set /p SRVPID=<"%OUTDIR%\repl_server.pid"
taskkill /PID %SRVPID% /F
echo REPLICATION FAIL
endlocal
exit /b 1
