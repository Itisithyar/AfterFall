#!/usr/bin/env python3
"""AFTERFALL asset reference check (no engine).

Scans committed Content for /Game/ references inside active assets and
classifies each as READY (resolves to an active file), QUARANTINED (exists
only under *.bak, never cooked), or MISSING. Also reports the Characters
merge status (the template mannequin tree that repairs the broken links).

Mirrors the style of the other Scripts/validate_*.py checks:
prints [PASS]/[FAIL] lines and exits 1 on failure.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTENT = ROOT / "Content"
REF_RE = re.compile(rb"/Game/[A-Za-z0-9_/]+")

# Known quarantine set: must stay .bak (never active) until the in-editor
# retarget fix lands (HEAD 7f2bc2d).
QUARANTINED = {
    "Content/FirstPerson/Anims.bak/ABP_FP_Copy.uasset",
    "Content/FirstPerson/Anims.bak/CtrlRig_FPWarp.uasset",
}

# Binary extensions worth scanning for embedded /Game/ refs.
# (.ini config strings like DataLayers/FoliageTypes are benign leftovers
# and are excluded from the gate.)
SCAN_EXTS = {".uasset", ".umap"}


def game_ref_to_rel(ref: str):
    # "/Game/Foo/Bar" -> Content/Foo/Bar (+ extension resolved by caller).
    return pathlib.PurePosixPath(ref[len("/Game/"):])


def collect_refs():
    refs = {}  # ref -> set of files that mention it
    for path in sorted(CONTENT.rglob("*")):
        if not path.is_file():
            continue
        if ".bak" in path.parts:
            continue
        if path.suffix not in SCAN_EXTS:
            continue
        try:
            data = path.read_bytes()
        except OSError:
            continue
        for m in REF_RE.finditer(data):
            ref = m.group(0).decode("ascii", "replace")
            refs.setdefault(ref, set()).add(str(path.relative_to(ROOT)))
    return refs


def resolve(ref: str):
    """READY / QUARANTINED / MISSING for a /Game/ reference."""
    rel = game_ref_to_rel(ref)
    for ext in (".uasset", ".umap"):
        active = CONTENT / (rel.as_posix() + ext)
        if active.exists():
            return "READY"
    backup_hit = False
    for ext in (".uasset", ".umap"):
        cand = str(rel.as_posix() + ext)
        for qb in QUARANTINED:
            if qb.endswith("/Anims.bak/" + cand.split("/")[-1]) or \
                    qb == "Content/" + cand.replace("/Anims/", "/Anims.bak/", 1):
                p = ROOT / qb
                if p.exists():
                    backup_hit = True
    # Generic fallback: same filename exists anywhere under *.bak.
    if not backup_hit:
        name = rel.name
        for bak in CONTENT.rglob("*.bak"):
            if bak.is_dir():
                if (bak / (name + ".uasset")).exists() or \
                        (bak / (name + ".umap")).exists():
                    backup_hit = True
                    break
    return "QUARANTINED" if backup_hit else "MISSING"


def main():
    fails = 0

    def check(name, ok, detail=""):
        nonlocal fails
        print(f"[{'PASS' if ok else 'FAIL'}] {name}" +
              (f" ({detail})" if detail else ""))
        fails += 0 if ok else 1

    # 1. Quarantine intact: .bak files present, no active twins.
    for qb in sorted(QUARANTINED):
        qpath = ROOT / pathlib.PurePosixPath(qb)
        twin = qpath.parent.parent / qpath.parent.name.replace(".bak", "") / qpath.name
        check(f"quarantined {qpath.name}",
              qpath.exists() and not twin.exists(),
              "still .bak, no active twin")

    # 2. Reference audit (informational unless the Characters merge is
    #    present but incomplete — a half-merge must fail loudly).
    refs = collect_refs()
    unresolved = []
    for ref in sorted(refs):
        if ref.startswith("/Engine/"):
            continue
        status = resolve(ref)
        if status != "READY":
            users = sorted(pathlib.PureWindowsPath(u).as_posix() for u in refs[ref])
            unresolved.append((ref, status, users))
    chars = CONTENT / "Characters"
    merged = chars.is_dir()
    char_broken = [r for r in unresolved
                   if r[0].startswith("/Game/Characters/")]
    check("characters merge coherent", not (merged and char_broken),
          "not merged yet" if not merged else
          ("complete" if not char_broken else f"{len(char_broken)} broken"))
    for ref, status, users in unresolved:
        print(f"  [{status}] {ref} <- {', '.join(users)}")

    # 3. Characters merge status.
    n = sum(1 for _ in chars.rglob("*.uasset")) if merged else 0
    print(f"[INFO] Content/Characters: "
          f"{'merged (' + str(n) + ' uassets)' if n else 'NOT merged (template links stay broken)'}")

    print("ASSET REFS: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)


if __name__ == "__main__":
    main()
