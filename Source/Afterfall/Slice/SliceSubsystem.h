// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Slice) — map-agnostic first-playable wiring. Runs in ANY
// game world (template map included): spawns slice actors, swaps the
// template pawn for our C++ pawn, and offers a scripted self-test console
// command (afSliceSelfTest) that exercises the full loop in-engine.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SliceSubsystem.generated.h"

class AAfterfallCharacter;
class AResourceNode;
class AZombiePrototype;
class AStructuralActor;
enum class EStructuralPart : uint8;

UCLASS()
class AFTERFALL_API UAfterfallSliceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Respawn helper used by the character after death. */
	void RespawnPlayer(AController* PC);

	/** Scripted loop test: harvest -> zombie AI -> kill -> death/respawn. */
	void RunSliceSelfTest();

	/** Stage 2 loop test: fixed-step sprint, carry cap, pickup/throw, mat. */
	void RunStage2SelfTest();

	/**
	 * Runtime pipeline test (-afPipelineSelfTest): drives the PRODUCTION damage
	 * path (character melee + hitscan RPCs) into a slice-spawned structure and
	 * asserts state, collision, delta export and reload restoration. Exits the
	 * process with 0/1 so the shell can assert the outcome.
	 */
	void RunPipelineSelfTest();

	/** Defers until slice setup has spawned the runtime seed (console/flag safe). */
	void RequestPipelineSelfTest();

private:
	bool bSliceReady = false;
	FTimerHandle SetupTimer;
	FTimerHandle PipelineTimer;
	/** How many times the pipeline test deferred waiting for slice setup. */
	int32 PipelineDeferrals = 0;

	/** Destructible structure placed at setup (runtime seed for the pipeline). */
	TWeakObjectPtr<AStructuralActor> SeedWall;

	void DoSliceSetup();
	AAfterfallCharacter* FindSlicePawn() const;
	/** Production spawn path for structural parts (server only). */
	AStructuralActor* SpawnStructuralPart(EStructuralPart Part, const FVector& At,
		const FRotator& Rot, float ExtentCm);
	/** Line trace through one 3x3x3 cell of a part; returns what it hit. */
	AActor* TraceThroughCell(AStructuralActor* Part, int32 CellIndex, FString& OutDetail);
};
