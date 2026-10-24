#!/usr/bin/env python3
"""Mirror of UPersistenceService (no engine)."""
import hashlib, math

def stable_guid(seed, gen, key):
    src = f"{seed}:{gen}:{key}"
    h = hashlib.md5(src.encode()).hexdigest()
    return f"{h[:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"

def is_compatible(save_v, code_v):
    if save_v == code_v:
        return (True, False)
    if save_v < code_v:
        return (True, True)
    return (False, False)

def cell_key(x, y, size):
    return f"{math.floor(x/size)},{math.floor(y/size)}"

def should_tombstone(taken, stale, unique):
    return bool(taken or stale or unique)

def catchup(elapsed, rate, cap):
    return min(max(elapsed, 0) * max(rate, 0), cap) if elapsed > 0 and rate > 0 else 0.0

CASES = [
    ("same version", is_compatible(1, 1) == (True, False)),
    ("older migrates", is_compatible(0, 1) == (True, True)),
    ("newer rejected", is_compatible(2, 1) == (False, False)),
    ("guid deterministic", stable_guid(1337, 1, "house:0") == stable_guid(1337, 1, "house:0")),
    ("guid differs", stable_guid(1337, 1, "house:0") != stable_guid(1337, 1, "house:1")),
    ("tombstone taken", should_tombstone(True, False, False) is True),
    ("live stays", should_tombstone(False, False, False) is False),
    ("catchup clamp", catchup(1000, 1, 50) == 50),
    ("cell key", cell_key(250, -50, 200) == "1,-1"),
]

def main():
    fails = sum(0 if ok else 1 for _, ok in CASES)
    for name, ok in CASES:
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    print("PERSISTENCE MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
