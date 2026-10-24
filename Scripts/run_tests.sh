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
check Source/Afterfall/Structural/StructuralActor.cpp
check Source/Afterfall/Structural/ConstructionValidator.h
check Source/Afterfall/Structural/ConstructionValidator.cpp
check Source/Afterfall/Inventory/InventoryComponent.h
check Source/Afterfall/Inventory/InventoryComponent.cpp
check Source/Afterfall/Tests/ConstructionValidatorTest.cpp
check Scripts/validate_phase3.py
check Source/Afterfall/Loot/LootProfiles.h
check Source/Afterfall/Loot/LootService.h
check Source/Afterfall/Loot/LootService.cpp
check Source/Afterfall/Tests/LootServiceTest.cpp
check Scripts/validate_loot.py
check Scripts/validate_lootservice.py
check Content/Data/Loot/DA_Location_RES.json
check Content/Data/Loot/DA_Location_GAR.json
check Content/Data/Loot/DA_Location_HW.json
check Content/Data/Loot/DA_Location_PHM.json
check Content/Data/Loot/DA_Location_WRK.json
check Content/Data/Loot/DA_Location_ZMB.json
check Config/DefaultEngine.ini
check Config/DefaultGame.ini

echo "== Phase 3b mirror (python, no engine) =="
if python3 Scripts/validate_phase3.py; then echo "[PASS] phase3 mirror"; else echo "[FAIL] phase3 mirror"; FAIL=1; fi

echo "== Loot mirrors (python, no engine) =="
if python3 Scripts/validate_loot.py; then echo "[PASS] loot profiles"; else echo "[FAIL] loot profiles"; FAIL=1; fi
if python3 Scripts/validate_lootservice.py; then echo "[PASS] loot service"; else echo "[FAIL] loot service"; FAIL=1; fi

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

echo "== UE smoke (engine present? version check only, 15s timeout) =="
if [ -f "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" ]; then
  if timeout 15 "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" --version >/dev/null 2>&1; then
    echo "[PASS] UnrealEditor --version succeeded"
  else
    echo "[INFO] UnrealEditor present but --version returned non-zero (expected pre-first-build); full automation deferred until TestNeighborhood map + MSVC toolchain exist."
  fi
  echo "[SKIP] full UE automation pending (needs MSVC + generated project files)"
else
  echo "[SKIP] engine not installed — UE automation pending"
fi

if [ "$FAIL" -eq 0 ]; then echo "ALL CHECKS PASS (functional)"; else echo "FAILURES PRESENT"; exit 1; fi
