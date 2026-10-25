// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: FLINT (GLM session) —_runtime spawn hook, mirrors the project's
// established afPhysicsTest flag pattern: zero impact unless -afBoxWall is
// on the command line (graybox FR-013 runtime verification).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoxWallRuntimeSubsystem.generated.h"

UCLASS()
class AFTERFALL_API UBoxWallRuntimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Follows the PHYSICSTEST discipline: logs (never exits) when the flag
	 *  is absent; spawns the graybox demo box-building when present. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void SpawnGrayboxDemoBox();
};
