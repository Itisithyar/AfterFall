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
- Server-authoritative damage/repair (`ServerApplyDamage`/`ServerRepair`).
- 30Hz server tick, World Partition default, `RegionSeed=1337`.

## Definition of done (per milestone)
Two clients on a dedicated server see the same approved wall; off-grid requests rejected; no client-authoritative spawn (grep-guarded). Proc-gen, zombies, persistence, perf profiling land in Phases 4–7.
