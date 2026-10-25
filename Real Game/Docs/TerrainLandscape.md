# Afterfall Terrain → Unreal Landscape Bridge

Owner: Tech (Procedural). Status: conversion + layout math implemented and
tested (`TerrainLandscapeBridge`, `Afterfall.Terrain.Landscape.*` 4/4 pass).
No Landscape actor exists yet; no scene file touched.

## What uses native Landscape vs custom geometry

| Piece | Implementation |
|---|---|
| Primary ground surface (height, hills, valleys, ridges, plateaus) | Native `ALandscape` (planned) fed by Afterfall equations |
| Heightfield generation (regions, seed, H0, Stage A) | Custom Afterfall simulation (unchanged model) |
| Caves, overhangs, underground volumes | Custom 3D geometry (ProceduralMeshComponent, existing mesher) — Landscape is heightfield-only and cannot represent them |
| Surface collision | Native Landscape heightfield collision (free with the actor) |
| Cave collision | Custom PMC collision (as today) |

## Height encoding (engine-verified, UE 5.8 `LandscapeDataAccess.h`)

- `LocalZ = (U16 - 32768) / 128`, `WorldZ = LocalZ * ActorScaleZ`
- `U16 = clamp(round(WorldZ / ActorScaleZ * 128 + 32768), 0, 65535)`
- Landscape samples sit on a 100 cm local grid → world spacing = 100 cm × ActorScaleXY.
- Planned setup: ActorScale **(0.5, 0.5, 100)** → 50 cm samples, ±256 m height range, 0.78 cm quanta. Our tallest terrain (~75 m) fits with margin; anything past ±256 m rails by design (tested).

## Layout for the 2000 m world at 50 cm (resolver output, tested)

- **16×16 components, 1×1 sections of 255×255 quads** = 4080 quads/side = 2040 m covered (40 m extra for the edge falloff).
- Engine rule enforced (from `LandscapeEditorDetailCustomization_NewLandscape.cpp`): whole components below 8191 verts, max 32 components/side.
- Objective order: 1-section components (granularity) → fewest components → least waste.
- **Limit found:** 2000 m at 10 cm needs 20000 quads > the engine rule → rejected with an error. Detail work stays in the custom PMC path; Landscape carries the 50 cm world surface.

## Cave integration plan (not yet built)

- Cave volumes remain `UProceduralMeshComponent` meshes positioned in world space over the Landscape.
- Closed entrances first: cave mouths as shallow pits/sinkholes whose rims sit below the Landscape surface ring; verify visually that no gap or z-fighting appears at the seam.
- If a seamless hole is required later, evaluate Landscape visibility (hole) layers in an editor session — not assumed from headers.
- Collision: Landscape surface + PMC cave shells coexist; overlap is by construction (caves carve downward from the surface field).

## What the equations keep

Nothing in the Afterfall model changed: region hash selection, H0 shaping, Stage A/B updates, seed determinism, and the PMC marching-cubes tables are untouched. This bridge only encodes already-computed heights into Landscape's uint16 format.

## Next step

Create the `ALandscape` actor in an editor session (needs the `Landscape` module added to `Afterfall.Build.cs` first), import one generated heightmap via `SetHeightData`, and walk the surface — then cave mouths.
