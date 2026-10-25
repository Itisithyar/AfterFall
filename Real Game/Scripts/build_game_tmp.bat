@echo off
REM Temporary runner (BEAM): builds the Afterfall game target (not editor).
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Afterfall Win64 Development "%~dp0..\RealGame.uproject" -WaitMutex
echo BUILD_EXIT:%ERRORLEVEL%
