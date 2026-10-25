#!/usr/bin/env bash
# AFTERFALL prereq check — run before any build.
# Usage: bash Scripts/check_prereqs.sh
set -euo pipefail
PASS=1

echo "== AFTERFALL prereqs (UE 5.8, Windows client+server) =="

# 1. UE install (Epic stub at C:/Program Files/Epic Games/UE_5.8 is NOT enough)
if [ -f "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" ]; then
  echo "[OK] UnrealEditor.exe found"
else
  echo "[MISSING] UnrealEditor 5.8 not installed (only .egstore stub present)."
  echo "  Fix: Epic Games Launcher > Library > UE 5.8 > Install (need ~60GB free; you have ~133GB)."
  PASS=0
fi

# 2. MSVC C++ workload (two instances on this machine: Community IDE + C:/Program Files/VS toolchain).
# Prefer vswhere (any instance with VC.Tools), fall back to known paths.
MSVC_OK=0
if "/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath >/dev/null 2>&1; then
  MSVC_OK=1
fi
for d in "/c/Program Files/VS/VC/Tools/MSVC" "/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC"; do
  if ls "$d" >/dev/null 2>&1; then MSVC_OK=1; fi
done
if [ "$MSVC_OK" -eq 1 ]; then
  echo "[OK] MSVC toolchain found ($(ls /c/Program\ Files/VS/VC/Tools/MSVC/ 2>/dev/null | tr '\n' ' '))"
else
  echo "[MISSING] VS C++ workload not installed."
  echo "  Fix: Visual Studio Installer > Modify > check 'Desktop development with C++' + 'Windows 11 SDK'."
  PASS=0
fi

# 3. Project files
for f in RealGame.uproject Source/Afterfall.Target.cs Source/AfterfallServer.Target.cs Source/AfterfallEditor.Target.cs; do
  if [ -f "$f" ]; then echo "[OK] $f"; else echo "[MISSING] $f"; PASS=0; fi
done

# 4. Git LFS
if git lfs version >/dev/null 2>&1; then echo "[OK] git-lfs $(git lfs version)"; else echo "[MISSING] git-lfs"; PASS=0; fi

if [ "$PASS" -eq 1 ]; then echo "ALL PREREQS PASS"; else echo "BLOCKED — fix items above, then re-run."; exit 1; fi
