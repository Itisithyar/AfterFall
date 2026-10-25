#!/usr/bin/env python3
"""Mirror of UEntityLifecycleService (no engine)."""
def should_promote(hit, chase, encounter, flagged):
    return bool(hit or chase or encounter or flagged)

def can_demote(pursuer, dist, shared, unresolved):
    if pursuer or shared or unresolved:
        return False
    return dist > 100.0

def must_keep(dist):
    return dist <= 30.0

def valid_death_containers(n):
    return n == 1

CASES = [
    ("hit promotes", should_promote(True, False, False, False) is True),
    ("chase promotes w/o hit", should_promote(False, True, False, False) is True),
    ("encounter promotes", should_promote(False, False, True, False) is True),
    ("flag promotes", should_promote(False, False, False, True) is True),
    ("idle stays ghost", should_promote(False, False, False, False) is False),
    ("30m kept", must_keep(20) is True and must_keep(31) is False),
    ("pursuer blocks", can_demote(True, 500, False, False) is False),
    ("near blocks", can_demote(False, 50, False, False) is False),
    ("far clean demotes", can_demote(False, 150, False, False) is True),
    ("one container", valid_death_containers(1) is True and valid_death_containers(12) is False),
]

def main():
    fails = sum(0 if ok else 1 for _, ok in CASES)
    for name, ok in CASES:
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    print("ENTITY LIFECYCLE MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
