# AFTERFALL canonical entry points — `Real Game/` tree only

All paths are relative to `Real Game/`. Launchers use `%~dp0` (their own
location), so run them from anywhere; do not rely on the current directory.
Every command below was verified present on 2026-10-25 (branch
`config/boot-fix`). Canonical project: `RealGame.uproject` (UE 5.8).

| Operation | Command | Notes |
|---|---|---|
| Prerequisites check | `bash Scripts/check_prereqs.sh` | UE 5.8 + MSVC + SDK checks, exit code |
| Full verification (build editor + Afterfall.* suite + room + pipeline fixtures) | `Scripts\run_realgame_tests.bat` | Headless where possible; logs to `Saved\RoomTest\`; exit codes preserved |
| Editor build only | `UnrealBuildTool.exe AfterfallEditor Win64 Development -Project="%CD%\RealGame.uproject" -NoP4 -WaitMutex` | `-WaitMutex` serializes with other agents' builds |
| Game build only | `UnrealBuildTool.exe Afterfall Win64 Development -Project="%CD%\RealGame.uproject" -NoP4 -WaitMutex` | Output: `Binaries\Win64\Afterfall.exe` |
| Editor launch (this project) | `UnrealEditor.exe "%CD%\RealGame.uproject"` | Do not open a second editor on the same project while building |
| Play-in-editor (game mode) | `Scripts\PlayDev.bat` | PIE `-game` on `Lvl_FirstPerson?game=/Script/Afterfall.AfterfallGameMode` |
| Headless Afterfall.* test suite | `UnrealEditor-Cmd.exe "%CD%\RealGame.uproject" -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests Afterfall; Quit" -log -TestExit="Automation Test Queue Empty"` | Parse `Saved\Logs\RealGame*.log` for `Result={Success}` / `TEST COMPLETE` |
| Package + staged build + launch | `Real Game\BuildAndLaunchAfterfall.bat` | Run in a normal Windows terminal, NOT through an agent shell; needs no concurrent writer in `Saved\` |
| Launch packaged play copy | `Scripts\Play.bat` | Runs `Saved\Packaged\PlayCopy\Afterfall.exe` (only exists after packaging) |
| Python mirror validators | `python3 Scripts\validate_<area>.py` (13 files, no engine needed) | Fast file-level checks; exit 0 = pass |

Do NOT use the `*_tmp.bat`, `live1_*` or `run_*_tmp.bat` scripts as entry
points — they are other agents' scratch launchers. `build.sh` is the legacy
fork script; prefer the `.bat` chain on Windows.

Rules: one editor per project; close Live Coding before integration builds;
never wipe `Saved\` while `UnrealEditor-Cmd.exe` is running (owned cooks live
there); announce build windows in the agent inbox.
