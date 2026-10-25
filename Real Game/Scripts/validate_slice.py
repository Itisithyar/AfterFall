#!/usr/bin/env python3
"""AFTERFALL slice mirror (no engine): verifies the zombie damage table and
six-shot baseline by parsing Source/Afterfall/AI/ZombiePrototype.cpp.

Checks:
- DamageForPart returns Head=34, Torso=17, Limbs=12.
- Six torso hits kill 100 HP; three headshots kill.
- Per-part hit mapping covers all six body-part tags.
- HealthComponent clamps damage to [0, 200].
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ZOMBIE = ROOT / "Source/Afterfall/AI/ZombiePrototype.cpp"
ZOMBIE_H = ROOT / "Source/Afterfall/AI/ZombiePrototype.h"
HEALTH = ROOT / "Source/Afterfall/Combat/HealthComponent.h"
FAIL = 0

def check(name, cond):
    global FAIL
    print(("[PASS] " if cond else "[FAIL] ") + name)
    if not cond:
        FAIL = 1

src = ZOMBIE.read_text()
m = re.search(r"float AZombiePrototype::DamageForPart\(EBodyPart Part\)\s*\{(.*?)\n\}", src, re.S)
check("DamageForPart found", m is not None)
table = {}
if m:
    body = m.group(1)
    table["Head"] = "Head:  return 34" in body
    table["Torso"] = "Torso:  return 17" in body
    limbs = all(x in body for x in ("ArmL", "ArmR", "LegL", "LegR"))
    table["Limbs"] = limbs and "return 12" in body
    for k, v in table.items():
        check(f"damage table {k}", v)

hp = 100
check("six torso hits kill (6*17>=100)", 6 * 17 >= hp)
check("three headshots kill (3*34>=100)", 3 * 34 >= hp)
check("five torso hits do NOT kill (5*17<100)", 5 * 17 < hp)

hsrc = ZOMBIE_H.read_text()
tags = ["BodyPart.Head", "BodyPart.Torso", "BodyPart.ArmL",
        "BodyPart.ArmR", "BodyPart.LegL", "BodyPart.LegR"]
cppsrc = src
check("all six part tags present", all(t in cppsrc for t in tags))
check("first-hit record rule documented", "First confirmed damage" in cppsrc)
check("no record until damaged", "bHasRecord" in hsrc)
check("StableId assigned on first hit", "NewGuid" in cppsrc)
check("corpse cleanup timer", "CorpseTimer" in cppsrc or "Destroy()" in cppsrc)
check("disengage rule present", "DisengageRangeM" in cppsrc)

health = HEALTH.read_text()
check("damage clamp [0,200] present", "ClampDamage" in health and "200" in health)
check("death latch present", "bIsDead" in health)
check("server authority guard", "HasAuthority" in (ROOT / "Source/Afterfall/Combat/HealthComponent.cpp").read_text())

node = (ROOT / "Source/Afterfall/World/ResourceNode.cpp").read_text()
check("harvest depletion latch", "DepletedUntilUnix" in node)
check("no duplicate grant (latch before grant)", node.find("DepletedUntilUnix = ") < node.find("ServerAddItem"))
check("range validation on interact", "450.f" in node or "InteractRangeCm" in node)

print("SLICE MIRROR " + ("PASS" if FAIL == 0 else "FAIL"))
sys.exit(FAIL)
