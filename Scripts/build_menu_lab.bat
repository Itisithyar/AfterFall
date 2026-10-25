@echo off
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%~dp0..\RealGame.uproject" -WaitMutex
echo BUILD_EXIT:%ERRORLEVEL%
