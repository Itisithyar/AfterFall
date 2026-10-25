@echo off
REM Builds the Real Game target. Resolves the .uproject relative to this
REM script's folder, so a worktree checkout builds itself instead of
REM silently building the main folder (that hardcoded-path false-pass
REM hazard is why the path is printed below). Pass a .uproject path as
REM the first argument to override the default.
if not "%~1"=="" (
  set "UPROJ=%~1"
) else (
  set "UPROJ=%~dp0..\RealGame.uproject"
)
echo UPROJECT:%UPROJ%
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AfterfallEditor Win64 Development "%UPROJ%" -WaitMutex
echo BUILD_EXIT:%ERRORLEVEL%
