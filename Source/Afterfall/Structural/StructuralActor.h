// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Structural team — single actor class for generated + player-built parts.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructuralTypes.h"
#include "StructuralDestructionComponent.h"
#include "Destruction/DestructionPropagation.h"
#include "Persistence/RegionStateSave.h"
#include "StructuralActor.generated.h"

UCLASS()
class AFTERFALL_API AStructuralActor : public AActor
{
	GENERATED_BODY()

public:
	AStructuralActor();

	// Canonical spec — set ONLY by server (construction validation or proc-gen).
	UPROPERTY(ReplicatedUsing = OnRep_Spec, EditDefaultsOnly, Category = "Structural")
	FStructuralSpec Spec;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural")
	float Durability = 100.f;

	// Owning player / claimed building ID. Server-authoritative.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural|Ownership")
	int32 OwnerPlayerId = -1;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural|Ownership")
	bool bIsClaimed = false;

	// Upgrade tier 0=twig/stick stub, 1=wood, 2=reinforced (Phase 3 vertical slice: 0->1 only).
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural")
	int32 UpgradeTier = 0;

	// Stable persistent identity. Assigned by server init; survives unload/reload.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural|Persistence")
	FGuid StableGuid;

	/** Detached 3x3x3 grid cells (0..26). Replicated so visuals match authority. */
	UPROPERTY(ReplicatedUsing = OnRep_DetachedCells, VisibleAnywhere, Category = "Structural|Destruction")
	TArray<int32> DetachedCells;

	/** Local damage presentation (instanced cell grid). Visual only. */
	UPROPERTY(VisibleAnywhere, Category = "Structural|Destruction")
	UStructuralDestructionComponent* Destruction = nullptr;

	/** Material-failure propagation fired once per life (server-side gate). */
	UPROPERTY()
	bool bMaterialFailed = false;

	/** Bounded recent event ledger (cap 8) for event-level replay protection. */
	UPROPERTY()
	TArray<FGuid> RecentEventIds;

	// Server-only API. Called by GameMode/ConstructionSubsystem after validation.
	// Never call from client.
	void ServerInitialize(const FStructuralSpec& InSpec, int32 InOwnerId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerApplyDamage(float DamageAmount);

	/**
	 * Server: commit propagation damage idempotently. Each NEW cell costs
	 * DamagePerCell (durability) and is appended once; duplicate replays add
	 * nothing. Out-of-range cells are ignored. EventId gives event-level
	 * replay protection (bounded recent ledger); invalid id skips event dedupe
	 * but cell dedupe still applies.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerApplyDestruction(const TArray<int32>& Cells, float DamagePerCell, const FGuid& EventId);

	/**
	 * Server: run one validated impact through shared propagation and commit.
	 * Validates direction/energy/origin ownership; cells are NEVER trusted
	 * from the client — they come out of Propagate. Returns newly added cells.
	 */
	int32 ServerApplyImpact(const FDestructionEvent& Event, float DamagePerCell, const FGuid& EventId);

	/** Export authoritative damage/ownership for FRegionStateSave. Server only. */
	FStructureDelta ExportDelta() const;

	/** Restore authoritative damage/ownership after unload/reload. Server only. */
	void ImportDelta(const FStructureDelta& Delta);

	/** Rebuild local cells from authoritative DetachedCells (idempotent). */
	void RefreshDestructionVisual();

	// Repair costs 1 Wood per 50 missing durability (rounded up). Server validates resources.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRepair();

	// Upgrade 0->1 costs 2 Wood and doubles durability cap stub. Server validates.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUpgrade();

	// Claim an unclaimed generated building part. First-claimer wins (server orders).
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerClaim(int32 ClaimerPlayerId);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Spec();

	UFUNCTION()
	void OnRep_DetachedCells();

private:
	/** Shared server commit used by the RPC and the failure trigger. */
	int32 ApplyDestructionCells(const TArray<int32>& Cells, float DamagePerCell, const FGuid& EventId);

	/** Fires the one-shot gravity-driven failure propagation at zero durability. */
	void CheckMaterialFailure();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
