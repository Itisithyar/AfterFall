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

private:
	bool bSliceReady = false;
	FTimerHandle SetupTimer;

	void DoSliceSetup();
	AAfterfallCharacter* FindSlicePawn() const;
};
