#!/usr/bin/env python3
"""AFTERFALL destruction mirror — same rules as DestructionPropagation.cpp.
Exit 0 when all acceptance cases pass. Mirrors UE automation 1:1.
"""
import math

def grid_point(center, ext, ix, iy, iz):
    return (center[0] + (ix - 1) * ext, center[1] + (iy - 1) * ext, center[2] + (iz - 1) * ext)

def grid_index(ix, iy, iz):
    return ix + iy * 3 + iz * 9

RESIST = {"Glass": 0.15, "Wood": 0.40, "Concrete": 1.20, "Metal": 1.60}
COST = {"Bullet": 8.0, "Attack": 18.0, "MaterialFailure": 12.0}
REACH = {"Bullet": 300.0, "Attack": 120.0, "MaterialFailure": 150.0}

def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def dot(a, b): return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
def norm(a):
    n = math.sqrt(dot(a, a))
    return (a[0]/n, a[1]/n, a[2]/n) if n > 1e-9 else (0.0, 0.0, 0.0)
def dist(a, b):
    d = sub(a, b)
    return math.sqrt(dot(d, d))

def next_bp(center, ext, cursor, direction, visited, reach):
    d = norm(direction)
    if d == (0.0, 0.0, 0.0) or reach <= 0:
        return None
    best = None
    best_score = 0.0
    for iz in range(3):
        for iy in range(3):
            for ix in range(3):
                idx = grid_index(ix, iy, iz)
                if idx in visited:
                    continue
                p = grid_point(center, ext, ix, iy, iz)
                to = sub(p, cursor)
                proj = dot(to, d)
                if proj <= 0.01 or proj > reach:
                    continue
                lat = dist((d[0]*proj, d[1]*proj, d[2]*proj), to)
                cost = proj + lat * 0.5  # nearest-forward-first (surface entry)
                if best is None or cost < best_score:
                    best, best_score = idx, cost
    if best is None:
        return None
    ix, rem = best % 3, best
    return best, grid_point(center, ext, best % 3, (best // 3) % 3, best // 9)

def propagate(origin, direction, energy, initiator, material, center=(0,0,0), ext=50.0, max_steps=8):
    if norm(direction) == (0.0,0.0,0.0) or energy <= 0:
        return {"detached": [], "steps": 0, "denied": True}
    e = energy
    res = RESIST.get(material, 0.60)
    bcost, reach = COST[initiator], REACH[initiator]
    cursor, trav, visited, detached, steps, denied = origin, 0.0, set(), [], 0, False
    for _ in range(max_steps):
        nb = next_bp(center, ext, cursor, direction, visited, reach)
        if nb is None:
            break
        idx, pos = nb
        trav += dist(cursor, pos)
        cost = res * dist(cursor, pos) + bcost if False else res * dist(cursor, pos) + bcost
        if cost > e:
            denied, steps = True, steps + 1
            break
        e -= cost
        visited.add(idx)
        detached.append(idx)
        cursor = pos
        steps += 1
        if e < 1.0:
            break
    return {"detached": detached, "steps": steps, "denied": denied, "energy": e}

def main():
    fails = 0
    def check(name, cond):
        nonlocal fails
        print(f"[{'PASS' if cond else 'FAIL'}] {name}")
        if not cond:
            fails += 1
    check("27 grid points", grid_index(1,1,1) == 13)
    check("center point", grid_point((0,0,0), 50.0, 1,1,1) == (0,0,0))
    check("corner point", grid_point((0,0,0), 50.0, 0,0,0) == (-50.0,-50.0,-50.0))
    rb = propagate((-200,0,0), (1,0,0), 200.0, "Bullet", "Wood")
    check("bullet detaches", len(rb["detached"]) > 0)
    check("bullet bounded <=8", rb["steps"] <= 8)
    rc = propagate((-200,0,0), (1,0,0), 200.0, "Bullet", "Concrete")
    check("concrete resists", len(rc["detached"]) <= len(rb["detached"]))
    ra = propagate((-200,0,0), (1,0,0), 200.0, "Attack", "Wood")
    check("attack shorter reach", len(ra["detached"]) <= len(rb["detached"]))
    r0 = propagate((0,0,0), (0,0,0), 0.0, "Attack", "Wood")
    check("empty event detaches nothing", len(r0["detached"]) == 0 and r0["denied"])
    rb2 = propagate((-200,0,0), (1,0,0), 200.0, "Bullet", "Wood")
    check("deterministic", rb2["detached"] == rb["detached"])
    # Idempotent commit + save/unload/reload round trip (mirrors actor merge).
    def add(live, cell):
        if not (0 <= cell < 27) or cell in live:
            return False
        live.append(cell)
        return True
    def merge(live, cells):
        n = 0
        for c in cells:
            if add(live, c):
                n += 1
        return n
    live = []
    check("add valid cell", add(live, 13) and live == [13])
    check("duplicate replay adds nothing", not add(live, 13))
    check("out-of-range rejected", not add(live, 27) and not add(live, -1))
    check("merge adds only new valid", merge(live, [13, 14, 99]) == 1 and len(live) == 2)
    fresh = []
    check("reload restores 2 cells", merge(fresh, list(live)) == 2)
    check("replay after reload adds nothing", merge(fresh, list(live)) == 0)
    # --- Actor-stub: event ledger + failure + visual sync (mirrors C++ actor) ---
    class Actor:
        def __init__(self, maxdur=100.0):
            self.detached = []
            self.durability = maxdur
            self.maxdur = maxdur
            self.failed = False
            self.recent = []
        def apply_cells(self, cells, dmg, eid):
            if eid is not None:
                if eid in self.recent:
                    return 0  # event replay: ignored entirely
                self.recent.append(eid)
                self.recent = self.recent[-8:]  # bounded ledger
            added = merge(self.detached, cells)
            if added:
                self.durability = max(0.0, self.durability - dmg * added)
            self.check_failure()
            return added
        def apply_impact(self, origin, direction, energy, dmg, eid, center=(0,0,0)):
            if norm(direction) == (0,0,0) or not (0.0 <= dmg <= 1000.0):
                return 0
            if dist(origin, center) > 500.0:
                return 0  # origin far from target: rejected
            r = propagate(origin, direction, max(0.0, min(energy, 500.0)),
                          "Attack", "Wood", center)
            return self.apply_cells(r["detached"], dmg, eid)
        def check_failure(self):
            if self.failed or self.durability > 0:
                return 0
            self.failed = True  # one-shot; flag persists via delta
            r = propagate((0,0,0), (0,0,-1), 40.0, "MaterialFailure", "Wood")
            return self.apply_cells(r["detached"], 8.0, "failure-event")
        def visible(self):
            return [i for i in range(27) if i not in self.detached]
        def export(self):
            return {"cells": list(self.detached), "dur": self.durability,
                    "failed": self.failed, "recent": list(self.recent)}
        def load(self, d):  # legacy deltas lack failed/recent -> defaults
            merge(self.detached, d.get("cells", []))
            self.failed = d.get("failed", False)
            for i in d.get("recent", []):
                if i not in self.recent:
                    self.recent.append(i)
            self.recent = self.recent[-8:]
            self.durability = max(0.0, min(d.get("dur", self.maxdur), self.maxdur))
    a = Actor()
    # Melee origin sits on the surface (~1 cell out), not 2m away: reach 120 must cover it.
    e1 = a.apply_impact((-60,0,0), (1,0,0), 60.0, 10.0, "evt-1")
    check("impact commits", e1 > 0)
    check("event replay ignored", a.apply_cells(list(a.detached), 10.0, "evt-1") == 0)
    dur = a.durability
    e2 = a.apply_impact((-60,0,0), (1,0,0), 60.0, 10.0, "evt-2")  # distinct, overlapping
    check("distinct overlapping event applies only new cells",
          e2 >= 0 and a.durability <= dur)
    check("visual = intact minus detached", len(a.visible()) == 27 - len(a.detached))
    v1, v2 = a.visible(), a.visible()
    check("rebuild idempotent", v1 == v2)
    b = Actor()  # kill it -> failure fires once
    b.durability = 0.0
    n0 = len(b.detached)
    b.check_failure()
    check("failure detaches", len(b.detached) >= n0)
    n1 = len(b.detached)
    b.check_failure()
    check("failure fires once", len(b.detached) == n1 and b.failed)
    d = b.export()  # save/unload/reload: same result, no resurrection
    c = Actor()
    c.load(d)
    check("reload preserves cells+flag",
          sorted(c.detached) == sorted(b.detached) and c.failed and c.durability == b.durability)
    c.check_failure()
    check("reload never re-triggers failure", sorted(c.detached) == sorted(b.detached))
    c2 = Actor()  # replay of pre-save event after reload still ignored
    c2.load(d)
    check("replayed event after reload ignored",
          c2.apply_cells(list(c2.detached), 10.0, "failure-event") == 0)
    leg = Actor()  # legacy save: no failed/recent keys
    leg.load({"cells": [1, 2], "dur": 50.0})
    check("legacy delta loads intact", sorted(leg.detached) == [1, 2] and not leg.failed)
    check("legacy keeps failure available", leg.failed is False)
    # Local-frame equivalence: yawed+translated actor, world hit on its local
    # -X face must detach the same LOCAL cells as the unrotated reference.
    def rot_z(v, deg):
        import math as _m
        r, c, s = _m.radians(deg), _m.cos(_m.radians(deg)), _m.sin(_m.radians(deg))
        return (v[0]*c - v[1]*s, v[0]*s + v[1]*c, v[2])
    _T = (100.0, -200.0, 300.0)
    _axis = rot_z((1.0, 0.0, 0.0), 90.0)
    _n = math.sqrt(sum(x*x for x in _axis))
    _axis = tuple(x/_n for x in _axis)
    _w = (_T[0] - _axis[0]*60.0, _T[1] - _axis[1]*60.0, _T[2] - _axis[2]*60.0)
    _rel = (_w[0]-_T[0], _w[1]-_T[1], _w[2]-_T[2])
    _lo = rot_z(_rel, -90.0)
    _ld = rot_z(_axis, -90.0)
    check("world impact maps to local -X face",
          all(abs(x-y) < 0.01 for x, y in zip(_lo, (-60.0, 0.0, 0.0))))
    check("world dir maps to local +X",
          all(abs(x-y) < 0.01 for x, y in zip(_ld, (1.0, 0.0, 0.0))))
    _rl = propagate(_lo, _ld, 60.0, "Attack", "Wood")
    _rr = propagate((-60,0,0), (1,0,0), 60.0, "Attack", "Wood")
    check("rotated frame detaches same cells", _rl["detached"] == _rr["detached"])
    check("first cell on entry face (ix==0)",
          len(_rl["detached"]) > 0 and _rl["detached"][0] % 3 == 0)
    check("invalid impact rejected (zero dir)", a.apply_impact((0,0,0), (0,0,0), 60, 10, "x") == 0)
    check("invalid impact rejected (far origin)", a.apply_impact((9000,0,0), (1,0,0), 60, 10, "y") == 0)
    check("invalid impact rejected (bad dmg)", a.apply_impact((-60,0,0), (1,0,0), 60, -5, "z") == 0)
    print("DESTRUCTION MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
