# Afterfall Dependency Map (Phase: Architecture Integration)

## Modules
- Afterfall (game) — Source/Afterfall/Afterfall.Build.cs → Core, CoreUObject, Engine, InputCore, EnhancedInput, NetCore
- Targets: Afterfall, AfterfallServer, AfterfallEditor

## Subsystems
- Gameplay: AfterfallCharacter, AfterfallGameMode, AfterfallHUD
- Structural: StructuralActor, StructuralTypes, ConstructionValidator, StructuralDestructionComponent
- Physics/World: PhysicalItem (Source/Afterfall/World/), CarryComponent, InteractComponent
- Destruction: DestructionRoomSubsystem (Source/Afterfall/Slice/), DestructionPropagation
- Persistence: RegionStateSave
- AI: ZombiePrototype

## Engine-independent (reusable)
- ConstructionValidator (pure, unit-tested)
- StructuralTypes (FStructuralSpec, FPlacementRequest)

## Unreal-bound
- All AActor/UActorComponent subclasses, replication, physics

## Integration points
- Server authority: ServerRequestPlacement/ServerApplyDamage/ServerRepair (grep-guarded)
- Carry: ServerThrowHeld/ServerDropHeld/ServerDeployMat
- Destruction: ServerApplyImpact → propagation → commit → replication

## Physics verification status
- Headless run of `afPhysicsTest` did not produce PHYSICSTEST output in the current log; execution stopped during asset loading.
- A separate `PHYTEST` run exists in logs (28 pass / 5 fail) from another agent's binary; source not located in this repo.
- `Source/Afterfall/Physics/PhysicsTestSubsystem.cpp` compiles (build succeeded).
- Runtime verification: NOT VERIFIED.
- Physics test subsystem (Source/Afterfall/Physics/) using /Engine/BasicShapes/Cube with console command afPhysicsTest. Status: compiles; runtime not yet verified (headless run hung during asset loading).

## Ownership
- Gameplay: Character/GameMode
- Structural: Structural/*
- Tech: Config/Scripts/Targets
