# AFTERFALL — World asset manifest

## Assets actually used by the playable world

| Asset | Path | Source | License | Notes |
|---|---|---|---|---|
| Cube | `/Engine/BasicShapes/Cube.Cube` | Unreal Engine 5.8 | Epic EULA (engine content) | ground, road, props, blockout shells |
| Cylinder | `/Engine/BasicShapes/Cylinder.Cylinder` | Unreal Engine 5.8 | Epic EULA | zombie limbs; lab block |
| Sphere | `/Engine/BasicShapes/Sphere.Sphere` | Unreal Engine 5.8 | Epic EULA | zombie head |
| Default material | engine default / dynamic instance | Unreal Engine 5.8 | Epic EULA | colour tint via `Color` parameter when available |

**No third-party asset has been downloaded or imported.** `Content/AF_External/`
does not exist yet; it is the reserved destination for authorized packs
(`Content/AF_External/<Category>/<PackName>/`).

## Project content on disk

```
Content/FirstPerson/Blueprints/BP_FirstPersonCharacter.uasset      (template, ABP_FP_Copy missing)
Content/FirstPerson/Blueprints/BP_FirstPersonGameMode.uasset       (template; reparented to AfterfallGameMode)
Content/FirstPerson/Blueprints/BP_FirstPersonPlayerController.uasset (template)
Content/FirstPerson/Lvl_FirstPerson.umap                           (template map, used as the host map)
```

## Asset acquisition audit (what was checked)

* The installed engine has **no loose `Engine/Content/Characters`** directory, so
  the UE mannequin cannot be reused from disk.
* The project has no skeletal content of any kind (see
  `Docs/ANIMATION_DEPENDENCY_MANIFEST.md`).
* No marketplace/Fab download was attempted: every candidate requires a license
  decision and an operator-authorized import, which this pass did not have.

## Consequences

The world is a **functional blockout**: geometry scale, layout, lighting,
collision and destruction are real and verified; art quality is not. Replacing
the blockout with imported meshes is a content task that does not require
changing the destruction or builder code — the builder spawns
`AStructuralActor` parts, and only the visual mesh/material source changes.
