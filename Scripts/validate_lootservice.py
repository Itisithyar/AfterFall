#!/usr/bin/env python3
"""Mirror of ULootService pure helpers (no engine). Exit 0 when all pass."""
import math

def pick_entry_count(lo, hi, t):
    if hi <= lo:
        return lo
    t = min(max(t, 0.0), 0.9999)
    return lo + math.floor(t * (hi - lo + 1))

def pick_rarity(grade, r01):
    order = ["Common", "Uncommon", "Rare", "VeryRare"]
    r = min(max(r01, 0.0), 0.9999) * 100.0
    acc = 0.0
    for k in order:
        acc += grade.get(k, 0.0)
        if r < acc:
            return k
    return order[-1]

def pick_category(mix, allow, r01):
    total = sum(mix.get(c, 0.0) for c in allow)
    if total <= 0:
        return None
    r = min(max(r01, 0.0), 0.9999) * total
    for c in allow:
        r -= mix.get(c, 0.0)
        if r < 0:
            return c
    return allow[-1]

def can_mint_unique(live, cap):
    return cap > 0 and live < cap

def apply_guarantee(items, any_of, chance, r01):
    if any(i in any_of for i in items):
        return -1
    if r01 >= chance or not items:
        return -1
    return len(items) - 1

CASES = [
    ("count min edge", pick_entry_count(1, 3, 0.0) == 1),
    ("count max edge", pick_entry_count(1, 3, 0.999) == 3),
    ("count fixed", pick_entry_count(2, 2, 0.5) == 2),
    ("rarity common", pick_rarity({"Common": 70, "Uncommon": 25, "Rare": 4.5, "VeryRare": 0.5}, 0.1) == "Common"),
    ("rarity uncommon", pick_rarity({"Common": 70, "Uncommon": 25, "Rare": 4.5, "VeryRare": 0.5}, 0.8) == "Uncommon"),
    ("rarity rare", pick_rarity({"Common": 70, "Uncommon": 25, "Rare": 4.5, "VeryRare": 0.5}, 0.97) == "Rare"),
    ("rarity vr", pick_rarity({"Common": 70, "Uncommon": 25, "Rare": 4.5, "VeryRare": 0.5}, 0.999) == "VeryRare"),
    ("category zero-mask", pick_category({"FOOD": 28}, ["WPN"], 0.5) is None),
    ("category weighted", pick_category({"FOOD": 28, "MED": 8}, ["FOOD", "MED"], 0.9) == "MED"),
    ("unique first mints", can_mint_unique(0, 1) is True),
    ("unique second blocked", can_mint_unique(1, 1) is False),
    ("guarantee replaces", apply_guarantee(["a", "b"], ["c"], 1.0, 0.0) == 1),
    ("guarantee satisfied", apply_guarantee(["a", "b"], ["a"], 1.0, 0.0) == -1),
]

def main():
    fails = 0
    for name, ok in CASES:
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
        fails += 0 if ok else 1
    print("LOOT SERVICE MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
