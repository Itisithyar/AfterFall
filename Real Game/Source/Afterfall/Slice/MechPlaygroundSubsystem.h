// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Muse (Mechanics) — dedicated development test area.
//
// A runtime-spawned mechanics playground on the running map (Entry by
// convention). No .umap is created or modified: everything spawns at runtime
// under the "MechPlay" tag and can be cleared + rebuilt (repeatable reset).
// Covers: floor, destroyable building (existing structural path), resource
// node, items, zombie presence, grapple attach/release, impact + detach,
// and the reset cycle. Headless entry: -afMechSelfTest (server) or the
// afMechSelfTest console command. Follows DestructionRoomSubsystem patterns.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MechPlaygroundSubsystem.generated.h"

class AStructuralActor;

UCLASS()
class AFTERFALL_API UMechPlaygroundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Build (or rebuild) the playground. @return false on clients. */
	bool EnsurePlayground();

	/** Destroy every tagged playground actor. @return destroyed count. */
	int32 ClearPlayground();

	/** Full self-test: build, assert, impact, grapple, reset. Exits on flag. */
	bool RunMechSelfTest(bool bAutoExit);

	/** Tag applied to every playground actor (reset + idempotence). */
	static FName PlaygroundTag() { return TEXT("MechPlay"); }

private:
	bool bAutoRunArmed = false;
	UFUNCTION()
	void RunAutoTest();
	/** Building parts spawned by the last Ensure (impact + reset checks). */
	TArray<TWeakObjectPtr<AStructuralActor>> Building;
	/** First glass/window part, used by the breakable-glass assertion. */
	TWeakObjectPtr<AStructuralActor> FirstWindow;
	int32 StructuralPartCount = 0;
	int32 PropCount = 0;
	int32 BuildingCount = 0;
	FVector AnchorCached = FVector::ZeroVector;
};
