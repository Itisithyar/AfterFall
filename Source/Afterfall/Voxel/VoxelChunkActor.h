// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — ONE authoritative 32^3 chunk with a ProceduralMesh
// component. Server commits voxel edits; clients receive the compact voxel
// data (never mesh buffers) and rebuild their own mesh + collision.
//
// Replication model
//   * ResetChunk (reliable multicast): the full 32KB voxel array + the
//     sequence it is current as of. Sent for late joins (a client joining a
//     live chunk has no edit history) and as the resync answer, so clients
//     converge without replaying every edit ever made.
//   * ApplyEdits (reliable multicast, batched once per frame): compact
//     incremental edits, each carrying its own Coord/State/Seq. Duplicates and
//     stale edits are rejected by Seq, so RPC retries cannot corrupt state.
//   * ServerSeq is replicated only as small instrumentation/UI state; the
//     authoritative voxel data itself never travels as actor properties.
//   * A new connection triggers a bounded burst of snapshots (the channel race:
//     PostLogin happens before the client holds this actor's channel).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Voxel/VoxelChunkTypes.h"
#include "Destruction/DestructionTypes.h"
#include "VoxelChunkActor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class AFTERFALL_API AVoxelChunkActor : public AActor
{
	GENERATED_BODY()

public:
	AVoxelChunkActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Chunk mesh: one PMC section, complex-as-simple collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
	UProceduralMeshComponent* ChunkMesh = nullptr;

	/** Server monotonic edit counter (replicated for instrumentation/UI). */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Voxel|Net")
	int32 ServerSeq = 0;

	/** Lattice origin of this chunk in world space (chunk (0,0,0) by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	FVector ChunkOrigin = FVector::ZeroVector;

	// ------------------------------------------------------------ server ---
	/** Deterministic initial content (bottom 8 layers solid stone). */
	void ServerInitializeChunk();

	/**
	 * Server: adopt externally-held voxel state (streaming restore). Used when a
	 * chunk was streamed out, so walking back does not resurrect the pristine
	 * content and undo the player's destruction. Rebuilds mesh + collision and
	 * pushes the adoption to connected clients.
	 */
	void ServerAdoptSnapshot(const FVoxelChunkData& Snapshot, int32 InSeq);

	/**
	 * Commit one voxel change. Rejects out-of-range coords, unknown material
	 * ids, and no-op writes. Bumps the sequence and queues the replicated edit.
	 */
	bool ServerApplyVoxelEdit(const FIntVector& Local, uint8 State);

	/** Batch commit; returns the number of applied edits. */
	int32 ServerApplyVoxelEdits(const TArray<FVoxelEdit>& Edits);
	bool ServerApplyFractureImpact(const FVector& ImpactPos, const FVector& ImpactDir, float Energy, uint32 Seed, EMaterialType Material);

	/**
	 * Push a full snapshot to every connected client (late join / resync).
	 * Reliable multicast to all clients holding this actor's channel.
	 */
	void ServerSendSnapshotToAll();

	const FVoxelChunkData& GetVoxelData() const { return VoxelData; }
	int32 GetServerSeq() const { return ServerSeq; }
	int32 GetLastAppliedSeq() const { return LastAppliedSeq; }
	int32 GetLastQuadCount() const { return LastQuadCount; }
	int32 GetMeshBuildCount() const { return MeshBuildCount; }

	/** Rebuild now instead of at the next tick (tests / explicit flush). */
	void FlushMeshRebuild();
	void MarkMeshDirty() { bMeshDirty = true; }

	/** Client: ask the authority for a full chunk snapshot via the local pawn. */
	void RequestResync();

	/**
	 * Multicast, not Client: this actor has no owning connection, and the engine
	 * DROPS Client RPCs on ownerless actors (NetDriver.cpp: "No owning connection
	 * ... will not be processed"). Multicast reaches every client that has the
	 * actor's channel open, which is exactly the relevance rule a world chunk
	 * needs. Runs locally on the server too, so both bodies no-op on authority.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyEdits(const TArray<FVoxelEdit>& Edits);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResetChunk(const FVoxelSnapshot& Snapshot);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Local authoritative (server) or replicated (client) voxel data. */
	FVoxelChunkData VoxelData;

	/** Highest edit sequence folded into VoxelData. */
	int32 LastAppliedSeq = 0;

	/** Server: edits committed this frame, flushed as one replicated batch. */
	TArray<FVoxelEdit> PendingEdits;

	/** Server: bounded recent-edit log (recovery/debug); not replicated. */
	TArray<FVoxelEdit> EditLog;

	bool bMeshDirty = true;
	int32 LastQuadCount = 0;
	int32 MeshBuildCount = 0;

/** Client resync pacing. */
double LastResyncRequestTime = -100.0;
/** Client: true once any authoritative snapshot has been adopted. */
bool bHasSnapshot = false;
/** Client: deferred resync pending until a local pawn becomes available. */
bool bResyncPending = false;
/** Client: one-shot tick instrumentation. */
bool bLoggedClientTick = false;

	/** Server: late-join snapshot burst after a connection count change. */
	int32 LastConnectionCount = 0;
	int32 SnapshotRetriesLeft = 0;
	double NextSnapshotTime = 0.0;

	void RebuildMesh();
};
