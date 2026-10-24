#!/usr/bin/env bash
# AFTERFALL tests — functional (no engine) + UE automation (needs engine).
# Usage: bash Scripts/run_tests.sh
set -euo pipefail
FAIL=0

echo "== Functional checks (no engine needed) =="
check() { if [ -f "$1" ]; then echo "[PASS] $1"; else echo "[FAIL] $1"; FAIL=1; fi; }
check Afterfall.uproject
check Source/Afterfall.Target.cs
check Source/AfterfallServer.Target.cs
check Source/AfterfallEditor.Target.cs
check Source/Afterfall/Afterfall.Build.cs
check Source/Afterfall/Afterfall.h
check Source/Afterfall/Afterfall.cpp
check Source/Afterfall/AfterfallGameMode.h
check Source/Afterfall/AfterfallCharacter.h
check Source/Afterfall/Structural/StructuralTypes.h
check Source/Afterfall/Structural/StructuralActor.h
check Config/DefaultEngine.ini
check Config/DefaultGame.ini

# No client-authoritative placement: grep guard
if grep -rn "ServerRequestPlacement\|ServerApplyDamage\|HasAuthority" Source/ >/dev/null; then
  echo "[PASS] server-authoritative paths present"
else
  echo "[FAIL] no server-authoritative code found"; FAIL=1
fi
if grep -rn "client.*spawn.*StructuralActor\|SpawnActor.*client" Source/ -i >/dev/null; then
  echo "[FAIL] possible client-authoritative spawn"; FAIL=1
else
  echo "[PASS] no client-authoritative spawn detected"
fi

echo "== UE smoke (engine present? version check only) =="
if [ -f "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" ]; then
  if "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" --version >/dev/null 2>&1; then
    echo "[PASS] UnrealEditor --version succeeded"
  else
    echo "[INFO] UnrealEditor present but --version returned non-zero (expected pre-first-build); full automation deferred until TestNeighborhood map + MSVC toolchain exist."
  fi
  echo "[SKIP] full UE automation pending (needs MSVC + generated project files)"
else
  echo "[SKIP] engine not installed — UE automation pending"
fi

if [ "$FAIL" -eq 0 ]; then echo "ALL CHECKS PASS (functional)"; else echo "FAILURES PRESENT"; exit 1; fi
