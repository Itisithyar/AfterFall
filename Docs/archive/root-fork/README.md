# Retired fork — quarantined files (2026-10-24)

The repository used to hold two divergent copies of the same game: the tracked
root project (`Afterfall.uproject`) and the untracked `Real Game/` project. On
2026-10-24 the Real Game tree was promoted to the repository root and the root
project was retired; the canonical project is now `RealGame.uproject`.

These files are the **retired fork's versions** of sources that also exist in the
promoted tree, kept only because they differ in behaviour rather than by
extension. They are not compiled, not referenced, and not part of the build.

| File | Why it is here |
|---|---|
| `Source/Afterfall/AfterfallCharacter.{h,cpp}` | older character: legacy `BindKey` input, client-direct `TryGrapple()`, no Enhanced Input, no quick menu. Superseded by the promoted `Source/Afterfall/AfterfallCharacter.*`. |
| `Source/Afterfall/Movement/AfterfallGrappleComponent.{h,cpp}` | the pre-port grapple (client-authoritative). The promoted copy adds server-authority refusal and replicated state. |
| `Source/Afterfall/AI/ZombiePrototype.{h,cpp}` | older AI variant (fewer states, no pursuit logging). |
| `Source/Afterfall/Afterfall.Build.cs` | old module deps (no ProceduralMeshComponent). |
| `Source/Afterfall/World/ResourceNode.cpp` | older resource-node variant. |
| `Source/Afterfall/Combat/EventDedup.h` | existed only in the retired fork; unreferenced there and here. |
| `Source/Afterfall/Physics/*`, `Slice/SliceSubsystem.*`, `Storage/PhysicalMat.cpp`, `Structural/StructuralDestructionComponent.cpp`, `Tests/RuntimeSimulationTest.cpp`, `UI/AfterfallHUD.*`, `UI/RadialMenuComponent.cpp`, `World/PhysicalItem.cpp` | earlier revisions of files the promoted tree supersedes. |

Everything identical between the two trees was simply dropped; only the
divergent copies were kept. If a future change needs a behaviour that only
exists here, port it deliberately — do not copy these files back wholesale.

Original tree: `git show 2995fd6^:Source/...` (the commit before consolidation).
