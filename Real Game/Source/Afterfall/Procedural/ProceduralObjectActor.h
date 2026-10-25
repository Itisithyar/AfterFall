// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — runtime instance of the universal
// procedural-object pipeline (rocks, props, buildings, terrain chunks,
// walls, organs of arbitrary shapes).
//
// Server-authoritative: clients send compact deform/fracture requests;
// the server validates, sequences, and replicates compact parameters +
// ordered deltas. Every machine rebuilds meshes locally from the same
// deterministic inputs — vertex buffers never travel the network.
// Unload keeps the delta (desc + ops + events); restore replays edits
// before collision is enabled, so streamed-out destruction is never
// resurrected to pristine state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Procedural/ProceduralObjectDesc.h"
#include "ProceduralObjectActor.generated.h"

class UProceduralMeshComponent;
struct FProceduralObjectDelta;

UCLASS()
class AFTERFALL_API AProceduralObjectActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralObjectActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Generated mesh. One section; collision policy from Desc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcObj")
	UProceduralMeshComponent* ProcMesh = nullptr;

	/** Serializable generation description (replicated, server-owned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "ProcObj")
	FProceduralObjectDesc Desc;

	/** Ordered committed deform ops (replicated, idempotent by OpId). */
	UPROPERTY(ReplicatedUsing = OnRep_Rebuild)
	TArray<FProceduralDeformOp> DeformOps;

	/** Ordered committed fracture events (replicated, idempotent by EventId). */
	UPROPERTY(ReplicatedUsing = OnRep_Rebuild)
	TArray<FProceduralFractureEvent> FractureEvents;

	/** Server monotonic edit counter (diagnostics/resync). */
	UPROPERTY(Replicated)
	int32 ServerSeq = 0;

	/** Last build stats (local diagnostics only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcObj")
	int32 LastVertexCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcObj")
	int32 LastTriangleCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcObj")
	int32 LastCarvedSamples = 0;

	// ---------------------------------------------------------- server ---
	/** Server: adopt a description (spawn path). Rejects on clients. */
	bool ServerAdoptDesc(const FProceduralObjectDesc& InDesc);

	/** Server: commit a deform op. Client supplies OpId; server assigns Seq. */
	bool ServerApplyDeform(const FProceduralDeformOp& Op);

	/**
	 * Server: commit a fracture impact. Energy in joules (caller computes
	 * 0.5*m*v^2 once). Rejected events (below threshold / invalid) return
	 * false and mutate nothing.
	 */
	bool ServerApplyFracture(const FVector& LocalPos, const FVector& LocalDir,
		float EnergyJ, EMaterialType Material, uint32 Seed, const FGuid& EventId);

	/** Server: adopt an externally-held delta (streaming restore). */
	bool ServerAdoptDelta(const FProceduralObjectDelta& Delta);

	// ------------------------------------------------------- replication --
	/** Client -> server deform request (server validates + sequences). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestDeform(const FProceduralDeformOp& Op);

	/** Client -> server fracture request (server validates + sequences). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestFracture(const FVector& LocalPos, const FVector& LocalDir,
		float EnergyJ, EMaterialType Material, uint32 Seed, const FGuid& EventId);

	// ------------------------------------------------------------ save ----
	/** Compact delta for region saves / streaming restore. */
	FProceduralObjectDelta GetDelta() const;

	/** Local rebuild from Desc + DeformOps + FractureEvents (game thread). */
	void RebuildNow();

protected:
	UFUNCTION()
	void OnRep_Rebuild();

private:
	/** Rebuild queued by replication; processed in Tick (one per frame). */
	bool bRebuildPending = false;
	/** Collision stays off until the first successful rebuild (no pristine interaction). */
	bool bCollisionLive = false;

	void MarkRebuildPending() { bRebuildPending = true; }
	bool HasOp(const FGuid& OpId) const;
	bool HasEvent(const FGuid& EventId) const;
};
