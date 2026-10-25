// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: FLINT (GLM session) — runtime wall/blocks actor for FR-013.
// Server-authoritative: block ledger, impact application (GUID-dedup like
// StructuralActor), visible detachment w/ rigid-body debris, support-loss
// collapse. Reuses UBoxBuildingLib (pure evaluation) and the existing
// BodyRegion mass model for thrown-body impacts. No second framework: this
// actor is the graybox runtime shell over BoxBuilding's data path.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Structural/BoxBuilding.h"
#include "Impact/ImpactDedup.h"
#include "BoxWallStructure.generated.h"

class UStaticMeshComponent;

/** One detached graybox block: rigid body echo with bounded lifetime and
 *  a bounded impact so momentum is plausible but never extreme. */
UCLASS()
class AFTERFALL_API ABoxBlockDebris : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	ABoxBlockDebris();
	/** Called by the wall after spawn; clamps impulse speed (graybox limit). */
	void ArmFrom(const FBoxBlock& Block, float MassKg,
		const FVector& ImpulseDir, float ImpactEnergyJ);
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY() FVector InheritedVelocity = FVector::ZeroVector;
	UPROPERTY() float LifeSeconds = 0.f;
	UPROPERTY() float BaseSpeedCmS = 0.f;
	static constexpr float MaxSpeedCmS = 350.f;    ///< 3.5 m/s cap
	static constexpr float LifeMaxSeconds = 12.f;  ///< debris budget
};

/** Graybox wall-slot cube mesh used by the runtime wall. */
USTRUCT()
struct FBoxWallRuntime
{
	GENERATED_BODY()

	UPROPERTY() FBoxBuildingProfile Profile;
	UPROPERTY() TArray<FBoxBlock> Blocks;
};

UCLASS()
class AFTERFALL_API ABoxWallStructure : public AActor
{
	GENERATED_BODY()

public:
	ABoxWallStructure();

	/** Server-only: set the profile and build the physical blocks. Cheap:
	 *  recomputes only on genuine profile change. */
	void ServerInitialize(const FBoxBuildingProfile& InProfile);

	/**
	 * Server-only legacy of the impact pathway (must be called from server
	 * contexts; the character's ServerAttack trace and physics-hit route
	 * both live in authority). GUID-deduplicated like StructuralActor.
	 * Returns the number of blocks newly detached/damaged.
	 */
	int32 ServerApplyBoxImpact(const FVector& WorldImpactPoint,
		const FVector& WorldDirection, const FBoxImpactInput& Impact,
		const FGuid& EventId);

	/** Blocks currently detached (server ledger; client reads via OnRep). */
	UPROPERTY(ReplicatedUsing = OnRep_BlockState, VisibleAnywhere,
		Category = "BoxWall")
	TArray<int32> DetachedBlockIds;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

	UFUNCTION()
	void OnRep_BlockState();

	/** Authority-only contact route for thrown/simulating bodies. */
	UFUNCTION()
	void OnBlockHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse,
		const FHitResult& Hit);

private:
	/** (Re)build per-block visual+collision components from the ledger. */
	void RebuildComponents();

	/** Apply EvalImpact ledger + spawn debris for removed/collapsed blocks. */
	void CommitResult(const FBoxImpactResult& Result, const FVector& DirWorld);

	/** FIX-B: remote clients spawn their own visual-only shard echo because
	 *  server-side physics debris is never assumed to appear on clients.
	 *  No-op on the authority (it already spawned the physical shard). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnDebrisEcho(int32 BlockId, float MassKg, FVector SpawnWorldPos,
		FVector ImpulseDir, float ImpactEnergyJ, int32 MaterialIdx);

	UPROPERTY(VisibleAnywhere, Category = "BoxWall")
	USceneComponent* Root;

	UPROPERTY(ReplicatedUsing = OnRep_BlockState, VisibleAnywhere, Category = "BoxWall")
	TArray<FBoxBlock> Blocks;
	UPROPERTY(Replicated, VisibleAnywhere, Category = "BoxWall")
	FBoxBuildingProfile Profile;
	UPROPERTY()
	TMap<int32, UStaticMeshComponent*> BlockComps;
	UPROPERTY()
	TArray<FGuid> RecentEventIds; ///< cap 8 like StructuralActor
	Afterfall::FImpactDedup ImpactDedup; ///< wall-level collision dedup
	UPROPERTY()
	TMap<int64, float> RecentImpactKeys; ///< key -> expiry (seconds); wall-level dedup
	bool bInitialized = false;

	UPROPERTY(EditDefaultsOnly, Category = "BoxWall")
	float ThrownBodyEnergyScale = 0.9f;   ///< graybox tuning: body->wall E share
};
