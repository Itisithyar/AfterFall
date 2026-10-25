# AFTERFALL — Playable world architecture (implemented)

Canonical project: `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject`
(The `Real Game\` folder has no `Source/`, `Config/` or `Content/` and is retired.)

## What actually exists

There is **no `.umap` authored for the playground**. The map assets in this
project are the UE template binaries (`/Game/FirstPerson/Lvl_FirstPerson`), and
this environment has no editor automation that can write a level asset. The
world is therefore **built at runtime in C++** by
`UMechPlaygroundSubsystem` (`Source/Afterfall/Slice/MechPlaygroundSubsystem.*`)
and is fully replaceable later by a real map with imported art.

Entry point: launch with `-afMechSelfTest` (headless run of the same builder +
assertions) or call the `afMechSelfTest` console command in PIE.

## Layout produced at runtime

Anchor = the `APlayerStart` in the running map (falls back to origin).

| Element | Construction | Count (last run) |
|---|---|---|
| Ground | `AStaticMeshActor` + `/Engine/BasicShapes/Cube`, 240 m x 240 m x 5 m, top at Z≈0 | 1 |
| Road | dark cube strip 180 m x 9 m along +X | 1 |
| Kerb line | thin cube strip | 1 |
| Props | crates, container, 6 fence sections, 4 debris slabs | 16 total incl. ground/road |
| Residence | 3 x 2 bays, 13.5 m x 9 m, doorway + windows | part of 191 structural parts |
| Service / commercial | 4 x 3 bays, 18 m x 13.5 m, doorway + windows | " |
| Industrial ruin | 4 x 2 bays, 17 m x 11 m, missing bays | " |
| Lighting | `ADirectionalLight` (intensity 3.2), `ASkyLight` (1.1), `AExponentialHeightFog` (density 0.02) | 3 |
| Gameplay | 1 `AResourceNode`, 2 `APhysicalItem`, 1 `AZombiePrototype` on the road | 4 |

Every spawned actor is tagged `MechPlay`, so `ClearPlayground()` removes the
whole world and `EnsurePlayground()` rebuilds it (verified by the fixture).

## Buildings

`MakeBuilding(Origin, SizeX, SizeY, bDamaged, DoorSideSign)` composes the shell
from **`AStructuralActor` parts** so every wall/window/door/roof segment is
already replicable and breakable through the production destruction path:

* floor: `EStructuralPart::Foundation` tiles
* roof: `EStructuralPart::Roof` tiles one part above the floor grid
* walls: `EStructuralPart::Wall` tiles; the door-side centre bay is left open as
  the entrance, and `EStructuralPart::Door` fills it until destroyed
* glass: outer bays of the long walls and the centre bays of the side walls are
  `EStructuralPart::Window` (durability 50 vs 90 for a wall, measured in the log)
* the industrial building skips bays (`(ix+side)%3`, `(iy+side)%4`) to read as a
  collapsed ruin

Cell extent is **100 cm** (one part covers 300 cm). This is load-bearing:
melee propagation reach is 120 cm (`InitiatorReachCm(Attack)`), so a part whose
cell grid sits 150 cm away can never be broken by a melee hit. That was found
and fixed during this pass.

## Navigation

No navmesh is generated: navigation is not required for the current AI
(straight-line steering with a stuck/sidestep fallback). Adding a
`NavMeshBoundsVolume` and a runtime navmesh build is the next world step if
pathfinding is wanted.

## Known gaps

* No authored `.umap`; no `DefaultEngine.ini` default-map change was made.
* Doors have no lock/animation: they are destructible parts, not hinged actors.
* Terrain is a flat blockout; there is no landscape or heightfield.
