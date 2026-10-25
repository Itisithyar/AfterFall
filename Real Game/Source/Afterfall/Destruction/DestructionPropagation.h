// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Destruction) — pure propagation rules. No tick, no physics,
// no persistence writes. Server evaluates, then applies via existing authority
// (e.g. AStructuralActor::ServerApplyDamage) and persists via the owning root.
#pragma once

#include "CoreMinimal.h"
#include "Structural/StructuralTypes.h"
#include "UObject/NoExportTypes.h"
#include "DestructionPropagation.generated.h"

/** Distinct initiators share the maze traversal but not the initial conditions. */
UENUM(BlueprintType)
enum class EDestructionInitiator : uint8
{
	Bullet          UMETA(DisplayName = "Bullet"),
	Attack          UMETA(DisplayName = "Melee/Attack"),
	MaterialFailure UMETA(DisplayName = "Material failure")
};

/** One initiating event. Caller supplies validated origin/direction/energy. */
USTRUCT(BlueprintType)
struct FDestructionEvent
{
	GENERATED_BODY()
	UPROPERTY() FVector Origin = FVector::ZeroVector;
	UPROPERTY() FVector Direction = FVector::ForwardVector;
	UPROPERTY() float InitialEnergy = 100.f;
	UPROPERTY() EDestructionInitiator Initiator = EDestructionInitiator::Attack;
	UPROPERTY() FName MaterialId = TEXT("Wood");
};

/** One evaluated step along the propagation path. */
USTRUCT(BlueprintType)
struct FPropagationStep
{
	GENERATED_BODY()
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() int32 BreakpointIndex = INDEX_NONE;
	UPROPERTY() float EnergyRemaining = 0.f;
	UPROPERTY() float DistanceTraversedCm = 0.f;
	UPROPERTY() bool bDetached = false;
	UPROPERTY() bool bDenied = false;
};

/** Bounded result. Detached indices are 3x3x3 grid slots the caller persists. */
USTRUCT(BlueprintType)
struct FPropagationResult
{
	GENERATED_BODY()
	UPROPERTY() TArray<FPropagationStep> Steps;
	UPROPERTY() TArray<int32> DetachedBreakpoints;
	UPROPERTY() bool bExhausted = true;
	UPROPERTY() bool bDenied = false;
	UPROPERTY() float EnergyRemaining = 0.f;
};

UCLASS()
class AFTERFALL_API UDestructionPropagationService : public UObject
{
	GENERATED_BODY()

public:
	/** 3x3x3 = 27 structural reference points. Indices 0..26. */
	static int32 GridPointCount() { return 27; }

	/** Grid slot (ix,iy,iz each 0..2) in world space. Center slot is (1,1,1)=index 13. */
	static FVector GridPoint(const FVector& Center, float ExtentCm, int32 IX, int32 IY, int32 IZ);

	static int32 GridIndex(int32 IX, int32 IY, int32 IZ) { return IX + IY * 3 + IZ * 9; }

	/** Documented placeholder resistance table (energy per cm). Data-driven later. */
	static float MaterialResistancePerCm(FName MaterialId);

	/** Extra energy cost to detach one breakpoint, per initiator. */
	static float InitiatorBreakpointCost(EDestructionInitiator Initiator);

	/** Max reach of one propagation, per initiator (bounds the maze walk). */
	static float InitiatorReachCm(EDestructionInitiator Initiator);

	/**
	 * Nearest reachable unvisited breakpoint with positive projection along
	 * Direction from CurrentPos, within ReachCm. False when none (= stop).
	 * Pure + directional: a sphere/radial query would NOT satisfy this.
	 * Nearest-first: damage enters at the surface and advances inward.
	 */
	static bool NextBreakpoint(const FVector& Center, float ExtentCm, const FVector& CurrentPos,
		const FVector& Direction, const TArray<int32>& Visited, float ReachCm,
		int32& OutIndex, FVector& OutPos);

	/**
	 * Bounded maze walk. At most MaxSteps iterations; stops on exhaustion
	 * (energy <= MinEnergy), denial (material cost > remaining), or no
	 * reachable breakpoint. Deterministic for fixed inputs.
	 *
	 * Frame-agnostic: Center/Origin/Direction must share ONE frame. Gameplay
	 * passes actor-local impacts (AStructuralActor converts on entry) with
	 * Center = ZeroVector, so indices match the actor-local visual grid under
	 * any rotation. Never mix a world impact with a local center.
	 */
	static FPropagationResult Propagate(const FDestructionEvent& Event, const FVector& Center,
		float ExtentCm, int32 MaxSteps = 8, float MinEnergy = 1.f);

	/** Documented gameplay energy/damage proxies (tunable; DT_Materials/weapons later). */
	static float AttackEnergy() { return 60.f; }
	static float BulletEnergy() { return 150.f; }
	static float FailureEnergy() { return 40.f; }
	static float AttackDamagePerCell() { return 10.f; }
	static float BulletDamagePerCell() { return 15.f; }
	static float FailureDamagePerCell() { return 8.f; }

	/** Part -> material id for resistance lookup (graybox mapping). */
	static FName MaterialForPart(EStructuralPart Part);

	/** Idempotent single-cell commit: validates 0..26, dedupes. Returns true if added. */
	static bool AddDetachedCell(TArray<int32>& InOutDetached, int32 Cell);

	/** Idempotent batch commit: returns number of newly added cells (0 = duplicate replay). */
	static int32 MergeDetached(TArray<int32>& InOutDetached, const TArray<int32>& NewCells);
};
