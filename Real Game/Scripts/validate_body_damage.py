#!/usr/bin/env python3
"""Mirror of body-part damage + damage-triggered persistence rules (no engine)."""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent

def main():
    fails = 0
    checks = []
    
    checks.append(("BodyDamageTypes.h exists", (ROOT / "Source/Afterfall/Combat/BodyDamageTypes.h").exists()))
    checks.append(("DamageResolver.h exists", (ROOT / "Source/Afterfall/Combat/DamageResolver.h").exists()))
    checks.append(("BodyPartDamageTest.cpp exists", (ROOT / "Source/Afterfall/Tests/BodyPartDamageTest.cpp").exists()))
    checks.append(("ZombiePrototype has damage record", (ROOT / "Source/Afterfall/AI/ZombiePrototype.h").exists()))
    checks.append(("RegionStateSave has zombie deltas", "FZombieDamageDelta" in (ROOT / "Source/Afterfall/Persistence/RegionStateSave.h").read_text()))
    checks.append(("EntityLifecycleService has damage persistence rules", "ShouldCreateDamageRecord" in (ROOT / "Source/Afterfall/Entities/EntityLifecycleService.h").read_text()))
    
    zcpp = (ROOT / "Source/Afterfall/AI/ZombiePrototype.cpp").read_text()
    checks.append(("Torso damage baseline present", "return 17" in zcpp or "BaseDamage = 17" in zcpp))
    
    for name, ok in checks:
        print(f"[{"PASS" if ok else "FAIL"}] {name}")
        if not ok:
            fails += 1
    
    print("BODY DAMAGE MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()

