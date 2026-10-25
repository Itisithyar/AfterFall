// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Physics) — headless rigid-body acceptance fixture.
// Runs the FIRST physics system (Chaos rigid bodies via UStaticMeshComponent)
// through quantitative checks: gravity free-fall vs d=1/2*g*t^2, mass,
// impulse J=m*dv, contact/settle, friction, plus the real pickup/throw
// release path and the character control defaults/handlers.
// Server-only, map-agnostic (spawns its own rig), exits nonzero on failure.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicsValidationSubsystem.generated.h"

class AStaticMeshActor;
class AAfterfallCharacter;
class APhysicalItem;
class UPhysicalMaterial;
class UStaticMeshComponent;

UCLASS()
class AFTERFALL_API UPhysicsValidationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Scripted acceptance run. Server only. Ends with a process exit. */
	void RunPhysicsSelfTest();

private:
	FTimerHandle LoopTimer;
	int32 Step = 0;
	int32 PassCount = 0;
	int32 FailCount = 0;

	// Rig actors (all spawned by the fixture, nothing from the map).
	AStaticMeshActor* Floor = nullptr;
	AStaticMeshActor* Body = nullptr;
	UStaticMeshComponent* BodyMesh = nullptr;
	UPROPERTY()
	UPhysicalMaterial* TestMat = nullptr;
	UPROPERTY()
	UStaticMesh* UnitCube = nullptr;
	AAfterfallCharacter* Pawn = nullptr;
	APhysicalItem* ItemA = nullptr;
	APhysicalItem* ItemB = nullptr;
	APhysicalItem* ItemC = nullptr;

	// Per-phase sampling state.
	double PhaseT = 0.0;
	double TotalT = 0.0;
	int32 PhaseFrames = 0;
	int32 TotalFrames = 0;
	float MaxPhysDt = 1.f / 30.f;
	double TestStartT = 0.0;
	bool bPawnModeSet = false;
	double RecZ0 = 0.0;
	double RecX0 = 0.0;
	double MinZ = 1e9;
	double QuietT = 0.0;
	double PawnT = 0.0;

	void Check(bool bOk, const TCHAR* Name, const FString& Detail);
	void StepTest();
	void FinishTest();
	void SetupRig();
	UStaticMeshComponent* SpawnBody(const FVector& At);
};
