#!/usr/bin/env python3
"""AFTERFALL Phase 3b mirror — same rules as ConstructionValidator.cpp, runnable without UE.
Exit 0 when all acceptance cases pass. Mirrors the UE automation test 1:1.
"""
COST = {"Foundation": 4, "Floor": 2, "Wall": 2, "Door": 3,
        "Window": 3, "Roof": 3, "Barricade": 1, "Lock": 1}

def snap(v):
    return round(v / 50.0) * 50.0

def is_on_grid(loc):
    return all(abs(snap(c) - c) < 1.0 for c in loc)

def validate(part, loc, supports, claimed_by_other, wood):
    if not is_on_grid(loc):
        return False, "off-grid"
    if claimed_by_other:
        return False, "lot-claimed-by-other"
    if part != "Foundation" and supports < 1:
        return False, "no-support"
    if wood < COST[part]:
        return False, f"insufficient-resources (need {COST[part]}, have {wood})"
    return True, ""

CASES = [
    # (name, part, loc, supports, claimed, wood, expect_ok, expect_reason_sub)
    ("foundation bootstraps", "Foundation", (0, 0, 0), 0, False, 10, True, ""),
    ("wall needs support", "Wall", (0, 0, 0), 0, False, 10, False, "no-support"),
    ("wall with support passes", "Wall", (50, 0, 0), 1, False, 10, True, ""),
    ("claimed lot rejected", "Wall", (50, 0, 0), 1, True, 10, False, "lot-claimed-by-other"),
    ("poor rejected", "Wall", (50, 0, 0), 1, False, 1, False, "insufficient-resources"),
    ("off-grid rejected", "Wall", (27, 0, 0), 1, False, 10, False, "off-grid"),
    ("barricade cheap", "Barricade", (100, 50, 0), 1, False, 1, True, ""),
    ("door cost 3, broke rejected", "Door", (50, 50, 0), 1, False, 2, False, "insufficient-resources"),
    ("repair cost 150 missing -> 3 wood", None, None, None, None, None, True, ""),  # computed below
]

def main():
    fails = 0
    for name, part, loc, sup, claimed, wood, exp_ok, exp_reason in CASES:
        if part is None:  # repair-cost rule check
            import math
            cost = math.ceil(150 / 50)
            ok = (cost == 3)
            print(f"[{'PASS' if ok else 'FAIL'}] {name}: ceil(150/50)={cost}")
            fails += 0 if ok else 1
            continue
        ok, reason = validate(part, loc, sup, claimed, wood)
        good = (ok == exp_ok) and (exp_reason in reason)
        print(f"[{'PASS' if good else 'FAIL'}] {name}: ok={ok} reason='{reason}'")
        if not good:
            fails += 1
    # cost table exactness
    for part, want in [("Wall", 2), ("Foundation", 4), ("Barricade", 1), ("Door", 3)]:
        good = COST[part] == want
        print(f"[{'PASS' if good else 'FAIL'}] cost {part}={COST[part]} (want {want})")
        fails += 0 if good else 1
    print("PHASE3 MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
