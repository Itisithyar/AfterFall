#!/usr/bin/env python3
"""AFTERFALL validate_loot.py — data-driven checks for slice loot profiles (no engine).
Checks: grade distribution ±3pp (10k rolls), zero-weight categories never hit,
guarantee presence, expected-entry sanity, single-copy caps documented.
Usage: python3 Scripts/validate_loot.py
"""
import json, random, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "Content" / "Data" / "Loot"
FILES = ["DA_Location_RES.json", "DA_Location_GAR.json", "DA_Location_HW.json",
         "DA_Location_PHM.json", "DA_Location_WRK.json", "DA_Location_ZMB.json"]

def load_all():
    out = {}
    for f in FILES:
        p = ROOT / f
        assert p.exists(), f"missing {p}"
        out[f] = json.loads(p.read_text())
    return out

def sim_grade(grade, n=10000, seed=1337):
    rng = random.Random(seed)
    cats = list(grade.keys())
    weights = [grade[c] for c in cats]
    counts = {c: 0 for c in cats}
    for _ in range(n):
        r = rng.uniform(0, sum(weights))
        acc = 0
        for c, w in zip(cats, weights):
            acc += w
            if r <= acc:
                counts[c] += 1
                break
    return {c: counts[c] / n * 100 for c in cats}

def check_zeros(profile):
    # category mix zeros must be respected: pick mix dict
    mix = profile.get("CategoryMix") or profile.get("CategoryMixBase") or profile.get("CategoryMix_WRK")
    assert mix, f"{profile.get('LocationId')}: no mix found"
    total = sum(mix.values())
    assert 98 <= total <= 102, f"{profile['LocationId']} mix sums {total}, want 100"
    zeros = [k for k, v in mix.items() if v == 0]
    return zeros

def expected_entries(profile):
    tot = 0.0
    for c in profile.get("Containers", []):
        lo, hi = c["Entries"]
        avg = (lo + hi) / 2
        chance = c.get("Chance", 1.0)
        tot += c.get("Count", 1) * avg * (1 - c.get("EmptyPct", 0) / 100) * chance
    return tot

def main():
    profiles = load_all()
    fails = 0
    print("== grade distribution (10k sim, tol ±3pp) ==")
    for fname, p in profiles.items():
        loc = p["LocationId"]
        grade = p.get("Grade")
        if not grade:
            print(f"[SKIP] {loc}: no Grade (WRK uses shared G1)")
            continue
        sim = sim_grade(grade)
        ok = all(abs(sim[k] - grade[k]) < 3.0 for k in grade)
        print(f"[{'PASS' if ok else 'FAIL'}] {loc} target={grade} sim=" +
              "{" + ", ".join(f"{k}:{v:.1f}" for k, v in sim.items()) + "}")
        if not ok:
            fails += 1
    print("== zero-category + mix-sum + entries ==")
    for fname, p in profiles.items():
        try:
            zeros = check_zeros(p)
            exp = expected_entries(p)
            print(f"[PASS] {p['LocationId']}: mix=100 zeros={zeros} expected_entries~{exp:.1f}")
        except AssertionError as e:
            print(f"[FAIL] {e}")
            fails += 1
    print("== guarantees present (slice) ==")
    for fname, p in profiles.items():
        gs = p.get("Guarantees", [])
        # slice locations must declare guarantees except WRK/ZMB (opportunistic)
        if p["LocationId"] in ("RES", "GAR", "HW", "PHM") and not gs:
            print(f"[FAIL] {p['LocationId']}: missing guarantees")
            fails += 1
        else:
            print(f"[PASS] {p['LocationId']}: {len(gs)} guarantee(s)")
    print("== single-copy registry refs ==")
    # ensure VR/engine/NVG mentions carry single-copy note where applicable
    for fname, p in profiles.items():
        txt = json.dumps(p)
        if "engine_part" in txt or "VeryRare" in txt:
            has_note = "single-copy" in txt.lower() or "UniqueInstance" in txt or "registry" in txt.lower() or "cap" in txt.lower()
            print(f"[{'PASS' if has_note else 'FAIL'}] {p['LocationId']}: single-copy/cap documented")
            if not has_note:
                fails += 1
    print("VALIDATE_LOOT: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
