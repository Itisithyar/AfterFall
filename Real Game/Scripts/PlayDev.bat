@echo off
REM Layered launcher: opens the map in-game (PIE -game) with AfterfallGameMode.
set "ROOT=%~dp0.."
set "UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64"
"%UE%\UnrealEditor.exe" "%ROOT%\RealGame.uproject" /Game/FirstPerson/Lvl_FirstPerson?game=/Script/Afterfall.AfterfallGameMode -game -windowed -log
