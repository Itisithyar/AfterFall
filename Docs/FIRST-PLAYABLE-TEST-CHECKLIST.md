# FIRST-PLAYABLE-TEST-CHECKLIST.md — exact steps (do not skip order)

## Post-install (once, ~30-60 min)
- [ ] 1. VS Installer → Modify Community → check Desktop development with C++ + Windows 11 SDK → reboot.
- [ ] 2. `bash Scripts/check_prereqs.sh` → expect ALL PASS (currently BLOCKED on MSVC).
- [ ] 3. Right-click `RealGame.uproject` → Generate Visual Studio project files.
- [ ] 4. `bash Scripts/build.sh editor` → expect success; record Output Log warnings.

## Open + Play (template map, interim)
- [ ] 5. Open `RealGame.uproject` in UE 5.8 (accept rebuild if prompted; should succeed after step 4).
- [ ] 6. Confirm `Content/FirstPerson/Lvl_FirstPerson` opens (it is `EditorStartupMap`).
- [ ] 7. Press Play (PIE, 1 player): WASD/mouse look, jump, collision with floor/walls. Record: works / fails + Output Log errors.
- [ ] 8. `stat unit` + `stat fps` in console → record avg FPS + frame ms in `Saved/Perf/` (baseline, not 165 claim).

## Slice map (after interim works)
- [ ] 9. Create `Content/Maps/TestNeighborhood.umap` (World Partition, 500m: lane + farm + garage + river + forest + tower).
- [ ] 10. Restore `DefaultEngine.ini` slice-target maps (uncomment 3 lines, remove interim 3).
- [ ] 11. Place 3 resource nodes (wood/stone/hemp placeholders) + 1 zombie spawner + trigger HUD text.
- [ ] 12. Re-run steps 7-8 on TestNeighborhood; file `FIRST-PLAYABLE-BACKLOG.md` items as done.

## Do-not-do
- Do not test PvP/zombies/loot/persistence in `Lvl_FirstPerson` — systems are code-only until step 11.
- Do not report FPS without pasting `stat unit` output + HW + preset.
