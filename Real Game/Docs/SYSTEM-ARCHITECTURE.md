# SYSTEM ARCHITECTURE — Afterfall (Consolidated)

Canonical project root: C:\Users\mckee\Desktop\Come Up Game
Unreal project: C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject
Engine: UE 5.8, target AfterfallEditor, config Development (Win64)

## Runtime module
- Afterfall (Runtime) — single module. Source under Source/Afterfall.
- Key systems: Character, Movement (UAfterfallMovementComponent), Inventory (UInventoryComponent), Health/Injury, Interaction, Carry, WorldGen, Destruction.

## Verified build
- Build.bat AfterfallEditor Win64 Development RealGame.uproject -> Succeeded (14.12s).

## Runtime vs dev
- Game runtime depends only on engine + project module. No HTTP/IPC to dev services found in inspected code.
- Dev ecosystem (scripts, backups, agents) runs alongside; not a game dependency.

## Systems
- Inventory: server-authoritative slots + equipment; GetWeightKg() sums items.
- Movement: load ratio from carried mass affects accel/speed/braking; sprint state machine.
- WorldGen: deterministic house placement with gap/overlap tests.
- Destruction: partial (propagator exists).
- UI: Canvas inventory HUD, radial interaction.

## Verification gaps
- No runtime/editor automation executed (headless limitation).
- Multiplayer not verified in this environment.
