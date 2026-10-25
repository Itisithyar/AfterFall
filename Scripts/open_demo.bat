@echo off
REM AFTERFALL one-click demo (run on your Windows PC with display, not headless).
REM Opens interim playable map in UE 5.8. C++ root needs MSVC build first;
REM falls back to Blueprint template map which runs without compile.
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe
if not exist "%UE%" (
  echo [MISSING] UE 5.8 editor not found at "%UE%"
  echo Install via Epic Games Launcher - Library - UE 5.8 - Install.
  pause & exit /b 1
)
echo [1/2] Opening template demo map (no compile needed)...
start "" "%UE%" "%~dp0..\RealGame.uproject" /Game/FirstPerson/Lvl_FirstPerson -game -log -windowed -ResX=1280 -ResY=720
echo [2/2] Canonical C++ project (needs MSVC build once):
echo   Right-click RealGame.uproject - Generate Visual Studio project files
echo   Then: bash Scripts/build.sh editor
echo   Then open RealGame.uproject - Play on Lvl_FirstPerson, later TestNeighborhood.
pause
