# AFTERFALL (UE 5.8, C++)

Stylized multiplayer survival vertical slice. See `Docs/ARCHITECTURE.md` + master brief §§1–7.

## Prereqs (Windows)
1. Epic Games Launcher → install **UE 5.8** (this folder currently has only the store stub).
2. Visual Studio Installer → Modify Community → **Desktop development with C++** + Windows 11 SDK.
3. Verify: `bash Scripts/check_prereqs.sh`

## Commands
```bash
bash Scripts/run_tests.sh            # functional checks (no engine needed)
bash Scripts/build.sh editor         # editor build (needs engine+MSVC)
bash Scripts/build.sh all            # editor + dedicated server
bash Scripts/launch_2client_server.sh # 1 server + 2 clients (TestNeighborhood)
```

## What works now (prototype)
- Shared `FStructuralSpec` (50cm grid) for proc-gen + player builds.
- Client preview (`BuildPreviewRequest`) + server-validated placement (`ServerRequestPlacement` → spawns replicated `AStructuralActor`).
- Full Phase 3b validation: grid → support overlap → ownership → cost → debit-then-spawn with refund.
- Server-authoritative wallet (`UInventoryComponent`, Wood=10 start), damage/repair (`ServerApplyDamage`/`ServerRepair`, 1 wood/50 hp), upgrade 0→1 (`ServerRequestUpgrade`, 2 wood), claim (`ServerClaim`, first wins).
- 30Hz server tick, World Partition default, `RegionSeed=1337`.
- Tests: `bash Scripts/run_tests.sh` (functional + `validate_phase3.py` 9/9 mirror pass) + UE `Afterfall.Construction.*` automation (needs editor build).

## Definition of done (per milestone)
Two clients on a dedicated server see the same approved wall; off-grid requests rejected; no client-authoritative spawn (grep-guarded). Proc-gen, zombies, persistence, perf profiling land in Phases 4–7.
