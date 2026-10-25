#!/usr/bin/env python3
"""AFTERFALL mechanic registry validator.

Reports compile / asset / runtime as SEPARATE states and never upgrades a state
on its own. It checks what can be checked from disk:

  * every declared C++ file exists
  * every declared module is enabled in the .uproject (or is an engine module)
  * every declared /Game asset path resolves to a file under Content/
  * /Engine assets are treated as engine-provided (not verified here)
  * declared skeleton / anim blueprint / physics asset presence

Usage:
    python Scripts/mechanic_registry.py [--json] [--root <project root>]
Exit code: 0 when no mechanic is BLOCKED on a missing *source* file, else 1.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ENGINE_MODULES = {
    "Engine", "Core", "CoreUObject", "InputCore", "EnhancedInput", "AIModule",
    "PhysicsCore", "ProceduralMeshComponent", "UMG", "Slate", "SlateCore",
}


def content_path(root: Path, ue_path: str) -> Path | None:
    """Map /Game/X/Y to Content/X/Y. Returns None for non-/Game paths."""
    if not ue_path.startswith("/Game/"):
        return None
    rel = ue_path[len("/Game/"):]
    base = root / "Content" / rel
    for suffix in (".uasset", ".umap"):
        if base.with_suffix(suffix).exists():
            return base.with_suffix(suffix)
    if base.exists():
        return base
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()
    root = Path(args.root).resolve()

    reg_path = root / "Docs" / "build_monitor" / "mechanics.registry.json"
    registry = json.loads(reg_path.read_text(encoding="utf-8"))
    uproj = json.loads((root / "RealGame.uproject").read_text(encoding="utf-8-sig"))
    enabled = {p["Name"] for p in uproj.get("Plugins", []) if p.get("Enabled")}

    rows = []
    blocked_sources = 0
    for m in registry["mechanics"]:
        problems: list[str] = []
        missing_src = [s for s in m.get("sources", []) if not (root / s).exists()]
        if missing_src:
            problems.append("missing source: " + ", ".join(missing_src))

        missing_mods = [mod for mod in m.get("modules", [])
                        if mod not in ENGINE_MODULES and mod not in enabled]
        if missing_mods:
            problems.append("module not enabled: " + ", ".join(missing_mods))

        missing_assets: list[str] = []
        engine_assets: list[str] = []
        for ref in m.get("animations", []):
            p = content_path(root, ref)
            if p is None:
                (engine_assets if ref.startswith("/Engine/") else missing_assets).append(ref)
        for key in ("skeleton", "animBlueprint", "physicsAsset"):
            ref = m.get(key)
            if not ref:
                continue
            if content_path(root, ref) is None:
                missing_assets.append(ref)
        if missing_assets:
            problems.append("missing asset: " + ", ".join(missing_assets))
        if engine_assets:
            problems.append("engine asset (not validated): " + ", ".join(engine_assets))

        status = m.get("status", {})
        asset_state = status.get("assets", "UNKNOWN")
        if missing_assets and asset_state not in ("MISSING",):
            asset_state = "MISMATCH (registry said %s, files missing)" % asset_state

        if missing_src or missing_mods:
            blocked_sources += 1
            verdict = "BLOCKED"
        elif missing_assets:
            verdict = "WAITING_FOR_ASSETS"
        else:
            verdict = "READY"

        rows.append({
            "id": m["id"],
            "verdict": verdict,
            "compile": status.get("compile", "UNKNOWN"),
            "assets": asset_state,
            "runtime": status.get("runtime", "UNKNOWN"),
            "problems": problems,
            "evidence": m.get("evidence", ""),
        })

    if args.json:
        print(json.dumps({"mechanics": rows}, indent=2))
    else:
        print("AFTERFALL build monitor - mechanic registry")
        print("=" * 74)
        for r in rows:
            print(f"[{r['verdict']:<17}] {r['id']}")
            print(f"    compile={r['compile']:<6} assets={r['assets']:<40} runtime={r['runtime']}")
            if r["problems"]:
                for p in r["problems"]:
                    print(f"    ! {p}")
            if r["evidence"]:
                print(f"    evidence: {r['evidence']}")
        ready = sum(1 for r in rows if r["verdict"] == "READY")
        wait = sum(1 for r in rows if r["verdict"] == "WAITING_FOR_ASSETS")
        blocked = sum(1 for r in rows if r["verdict"] == "BLOCKED")
        print("=" * 74)
        print(f"READY={ready} WAITING_FOR_ASSETS={wait} BLOCKED={blocked}")

    return 0 if blocked_sources == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
