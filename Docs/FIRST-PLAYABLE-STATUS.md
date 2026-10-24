# FIRST-PLAYABLE-STATUS.md — honest state (2026-10-24)

## Project identity
- Canonical: root `Afterfall.uproject` (UE 5.8, C++ `Afterfall` module, `AfterfallServer` target, EnhancedInput). Valid JSON, module present.
- Template source (ignored, not committed): `AfterFall/` Epic FirstPerson Blueprint project. Art merged to root `Content/FirstPerson/`, `Content/Input/`, `Content/LevelPrototyping/`, `Config/DefaultInput.ini`. `Characters/Mannequins/` (126MB) intentionally not merged.

## What exists
- C++ gameplay stubs (server-authoritative, unit-mirrored, NOT level-instanced): placement/validation (`ConstructionValidator`), wallet (`InventoryComponent` Wood + ItemId map), loot rolls (`LootService` + 6 location JSONs), delta saves (`RegionStateSave`), crafting (12 recipes), ghost/active/dormant lifecycle + death records.
- Playable content today: template `Content/FirstPerson/Lvl_FirstPerson.umap` (ground, lighting, sky, FP arms/character, collision) + `IMC_Default` input. No AFTERFALL zombies/resources/HUD instanced in any level yet.
- Config: `DefaultEngine.ini` now boots `Lvl_FirstPerson` (interim); 30Hz tick + WP default preserved. TestNeighborhood refs kept commented until map lands.
- Scripts: `check_prereqs.sh`, `build.sh`, `launch_2client_server.sh`, `run_tests.sh` (+5 python mirrors ALL PASS), `perf_baseline.sh` (Stage A).

## Verification actually run
- `validate_phase3/loot/lootservice/persistence/inventory_crafting/entity_lifecycle/perf_budget` — ALL PASS (no engine).
- `git status` clean before this change; `Afterfall.uproject` JSON parses; `Lvl_FirstPerson.umap` present; `TestNeighborhood` confirmed MISSING.
- NOT run (explicitly): generate project files, C++ compile, open editor, Play-in-Editor, zombie/resource/HUD behavior, FPS measurement. Blocked below.

## Blockers (precise)
1. **MSVC C++ workload missing** (`VC/Tools/MSVC/` absent). C++ canonical project cannot compile → editor prompts rebuild and fails. Fix: VS Installer → Modify → Desktop development with C++ + Win11 SDK.
2. **No AFTERFALL test map.** `TestNeighborhood.umap` does not exist. Interim boots template map; slice map is backlog item 1.
3. **No level-instanced gameplay.** Zombie/resource/HUD classes exist as C++/JSON only; nothing placed in `Lvl_FirstPerson`. Needs MSVC + editor pass.

## What changed in this handoff
- `Config/DefaultEngine.ini`: interim map → `Lvl_FirstPerson` (reversible, commented slice target).
- New: `FIRST-PLAYABLE-{STATUS,TEST-CHECKLIST,BACKLOG}.md` (this file + 2 companions). No gameplay code touched.
