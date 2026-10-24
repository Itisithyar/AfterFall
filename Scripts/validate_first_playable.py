#!/usr/bin/env python3
"""Mirror of slice yields + zombie tick gate + map spec presence (no engine)."""
import math, pathlib
def yld(kind, t):
    t = min(max(t, 0.0), 0.9999)
    return {"Wood": 3 + math.floor(t*3), "Stone": 4 + math.floor(t*5), "Hemp": 2 + math.floor(t*3)}[kind]

def tick_full(dist, pursuer, active=100.0):
    return True if pursuer else dist <= active

CASES = [
    ("wood max 5", yld("Wood", 0.999) == 5),
    ("stone max 8", yld("Stone", 0.999) == 8),
    ("hemp max 4", yld("Hemp", 0.999) == 4),
    ("30m persists/ticks", tick_full(20, False) is True),
    ("pursuer ticks far", tick_full(500, True) is True),
    ("far idle dormant", tick_full(500, False) is False),
]
ROOT = pathlib.Path(__file__).resolve().parent.parent
CHECKS = [
    ("uproject valid", (ROOT / "Afterfall.uproject").exists()),
    ("template map present", (ROOT / "Content/FirstPerson/Lvl_FirstPerson.umap").exists()),
    ("slice spec present", (ROOT / "Docs/TESTNEIGHBORHOOD-SPEC.md").exists()),
    ("status present", (ROOT / "Docs/FIRST-PLAYABLE-STATUS.md").exists()),
]

def main():
    fails = 0
    for n, ok in CASES + CHECKS:
        print(f"[{'PASS' if ok else 'FAIL'}] {n}")
        fails += 0 if ok else 1
    print("FIRST PLAYABLE MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
