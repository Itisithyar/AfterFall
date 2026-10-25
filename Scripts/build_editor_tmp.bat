@echo off
REM Builds the Real Game target. This file used to point at the SOURCE project
REM (..\Afterfall.uproject), which silently left Real Game binaries stale while
REM reporting BUILD_EXIT:0 - a false-pass hazard. Keep it on RealGame.uproject.
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject" -WaitMutex
echo BUILD_EXIT:%ERRORLEVEL%
