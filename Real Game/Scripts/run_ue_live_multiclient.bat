@echo off
REM AFTERFALL live multiclient destruction check (headless, no GUI required).
REM
REM 1. Builds AfterfallEditor.
REM 2. Starts a dedicated server (Entry map) with
REM    -afRoomServer -afRoomWaitClients=2: builds the room UNDAMAGED, waits
REM    for 2 stable client connections (+10s settle), then fires the three
REM    authoritative impacts live and logs ROOMSERVER READY. Log:
REM    live_server.out.
REM 3. Starts TWO clients (127.0.0.1 -game -nullrhi) with -afRoomLiveClient:
REM    each must first observe the INTACT baseline (proves it connected
REM    before destruction), then the live transition, then runs the full
REM    assertion set (state, visuals, collision, bypass-reject) and quits.
REM    Logs: live_client1.out, live_client2.out. A first sight that is
REM    already damaged FAILs loudly instead of passing falsely.
REM 4. Verifies both ROOMLIVE SUMMARY lines, confirms all three processes
REM    are gone (clients self-quit; server killed by captured PID only),
REM    and exits 0 on full pass, 1 otherwise.
setlocal
set UEDIR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64
set PROJ=%~dp0..\RealGame.uproject
set OUTDIR=%~dp0..\Saved\RoomTest
mkdir "%OUTDIR%" 2>nul
del "%OUTDIR%\live_server.out" 2>nul
del "%OUTDIR%\live_server.err" 2>nul
del "%OUTDIR%\live_client1.out" 2>nul
del "%OUTDIR%\live_client1.err" 2>nul
del "%OUTDIR%\live_client2.out" 2>nul
del "%OUTDIR%\live_client2.err" 2>nul
del "%OUTDIR%\live_server.pid" 2>nul
del "%OUTDIR%\live_client1.pid" 2>nul
del "%OUTDIR%\live_client2.pid" 2>nul

echo [1/5] Building AfterfallEditor...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%PROJ%" -WaitMutex
if ERRORLEVEL 1 (echo BUILD FAILED & exit /b 1)

echo [2/5] Starting dedicated server (waits for 2 clients)...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '\"%PROJ%\"', '/Engine/Maps/Entry', '-server', '-unattended', '-nopause', '-nosplash', '-nosound', '-afRoomServer', '-afRoomWaitClients=2', '-log', '-NoVerifyGC', '-stdout', '-FullStdOutLogOutput' -PassThru -RedirectStandardOutput '%OUTDIR%\live_server.out' -RedirectStandardError '%OUTDIR%\live_server.err'; $p.Id | Out-File -FilePath '%OUTDIR%\live_server.pid' -NoNewline -Encoding ascii"

echo [3/5] Starting client 1 and client 2...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '\"%PROJ%\"', '127.0.0.1', '-game', '-nullrhi', '-unattended', '-nopause', '-nosplash', '-nosound', '-afRoomLiveClient', '-log', '-NoVerifyGC', '-stdout', '-FullStdOutLogOutput' -PassThru -RedirectStandardOutput '%OUTDIR%\live_client1.out' -RedirectStandardError '%OUTDIR%\live_client1.err'; $p.Id | Out-File -FilePath '%OUTDIR%\live_client1.pid' -NoNewline -Encoding ascii"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '\"%PROJ%\"', '127.0.0.1', '-game', '-nullrhi', '-unattended', '-nopause', '-nosplash', '-nosound', '-afRoomLiveClient', '-log', '-NoVerifyGC', '-stdout', '-FullStdOutLogOutput' -PassThru -RedirectStandardOutput '%OUTDIR%\live_client2.out' -RedirectStandardError '%OUTDIR%\live_client2.err'; $p.Id | Out-File -FilePath '%OUTDIR%\live_client2.pid' -NoNewline -Encoding ascii"

set /a TRIES=0
:serverwait
ping -n 4 127.0.0.1 >nul
findstr /c:"ROOMSERVER READY" "%OUTDIR%\live_server.out" >nul
if ERRORLEVEL 1 goto servernotready
goto serverready
:servernotready
set /a TRIES+=1
if %TRIES% GEQ 80 goto cleanupfail
goto serverwait
:serverready
echo [4/5] Server fired live impacts. Waiting for both clients...
set /a TRIES=0
:clientwait
ping -n 4 127.0.0.1 >nul
findstr /c:"ROOMLIVE SUMMARY" "%OUTDIR%\live_client1.out" >nul
if ERRORLEVEL 1 goto clientnotdone
findstr /c:"ROOMLIVE SUMMARY" "%OUTDIR%\live_client2.out" >nul
if ERRORLEVEL 1 goto clientnotdone
goto clientdone
:clientnotdone
set /a TRIES+=1
if %TRIES% GEQ 80 goto cleanupfail
goto clientwait
:clientdone
echo [5/5] Both clients finished. Verifying and cleaning up...
	ping -n 16 127.0.0.1 >nul
set /p SRVPID=<"%OUTDIR%\live_server.pid"
set /p C1PID=<"%OUTDIR%\live_client1.pid"
set /p C2PID=<"%OUTDIR%\live_client2.pid"
set VERDICT=PASS
tasklist /FI "PID eq %C1PID%" 2>nul | findstr /c:"%C1PID%" >nul
if ERRORLEVEL 0 set VERDICT=CLIENT1-HANG
tasklist /FI "PID eq %C2PID%" 2>nul | findstr /c:"%C2PID%" >nul
if ERRORLEVEL 0 set VERDICT=CLIENT2-HANG
if not "%VERDICT%"=="PASS" goto dokill
findstr /c:"ROOMSERVER READY" "%OUTDIR%\live_server.out" >nul
if ERRORLEVEL 1 set VERDICT=NO-SERVER-READY
findstr /c:"ROOMLIVE SUMMARY" "%OUTDIR%\live_client1.out" >nul
if ERRORLEVEL 1 set VERDICT=NO-SUMMARY-1
findstr /c:"ROOMLIVE SUMMARY" "%OUTDIR%\live_client2.out" >nul
if ERRORLEVEL 1 set VERDICT=NO-SUMMARY-2
findstr /c:"ROOMLIVE FAIL" "%OUTDIR%\live_client1.out" >nul
if ERRORLEVEL 0 set VERDICT=FAIL-1
findstr /c:"ROOMLIVE FAIL" "%OUTDIR%\live_client2.out" >nul
if ERRORLEVEL 0 set VERDICT=FAIL-2
findstr /c:"ROOMLIVE PASS" "%OUTDIR%\live_client1.out" >nul
if ERRORLEVEL 1 set VERDICT=NO-PASS-1
findstr /c:"ROOMLIVE PASS" "%OUTDIR%\live_client2.out" >nul
if ERRORLEVEL 1 set VERDICT=NO-PASS-2
:dokill
if "%VERDICT%"=="CLIENT1-HANG" taskkill /PID %C1PID% /F
if "%VERDICT%"=="CLIENT2-HANG" taskkill /PID %C2PID% /F
taskkill /PID %SRVPID% /F
findstr /c:"ROOMSERVER READY" "%OUTDIR%\live_server.out"
findstr /c:"ROOMLIVE SUMMARY" "%OUTDIR%\live_client1.out"
findstr /c:"ROOMLIVE SUMMARY" "%OUTDIR%\live_client2.out"
if "%VERDICT%"=="PASS" goto verdictpass
echo LIVE MULTICLIENT FAIL (%VERDICT%)
endlocal
exit /b 1
:verdictpass
echo LIVE MULTICLIENT PASS
endlocal
exit /b 0
:cleanupfail
echo TIMEOUT WAITING - killing test processes only
set /p SRVPID=<"%OUTDIR%\live_server.pid"
taskkill /PID %SRVPID% /F
set /p C1PID=<"%OUTDIR%\live_client1.pid"
taskkill /PID %C1PID% /F 2>nul
set /p C2PID=<"%OUTDIR%\live_client2.pid"
taskkill /PID %C2PID% /F 2>nul
echo LIVE MULTICLIENT FAIL (timeout)
endlocal
exit /b 1
