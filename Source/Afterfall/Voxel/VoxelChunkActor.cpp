// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — single-chunk authoritative replication proof.
#include "Voxel/VoxelChunkActor.h"
#include "Voxel/VoxelBasis.h"
#include "Destruction/FractureSolver.h"
#include "AfterfallCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

AVoxelChunkActor::AVoxelChunkActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// Single-chunk proof: every client must have the authoritative chunk,
	// so relevance is unconditional (multi-chunk streaming is out of scope).
	bAlwaysRelevant = true;
	SetCanBeDamaged(false);

	ChunkMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ChunkMesh"));
	RootComponent = ChunkMesh;
	ChunkMesh->SetMobility(EComponentMobility::Movable);
	// Synchronous cooking keeps collision valid the instant the mesh is
	// rebuilt, so a replicated edit is collision-correct on the same frame.
	ChunkMesh->bUseAsyncCooking = false;
	ChunkMesh->bUseComplexAsSimpleCollision = true;
	ChunkMesh->SetCollisionProfileName(TEXT("BlockAll"));
	ChunkMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChunkMesh->SetGenerateOverlapEvents(false);
}

void AVoxelChunkActor::BeginPlay()
{
	Super::BeginPlay();
	if (!ChunkOrigin.IsZero())
	{
		SetActorLocation(ChunkOrigin);
	}
	if (HasAuthority())
	{
		if (!VoxelData.IsValid())
		{
			ServerInitializeChunk();
		}
	}
	else
	{
		// Client: the initial snapshot arrives as an initial-only property;
		// build from it (OnRep) and ask for a resync if it never lands.
		MarkMeshDirty();
		UE_LOG(LogAfterfall, Log, TEXT("VOXELCLIENT chunk actor begun (netmode=%d)"),
			(int32)GetNetMode());
	}
}

void AVoxelChunkActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Only the sequence number travels as an actor property (small, for
	// instrumentation). Voxel state moves through the batched snapshot/edit
	// RPCs so a 32KB array is never re-sent as a property delta.
	DOREPLIFETIME(AVoxelChunkActor, ServerSeq);
}

void AVoxelChunkActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (PendingEdits.Num() > 0)
		{
			// One batch per frame: several edits in the same frame cost a
			// single reliable RPC instead of one per voxel.
			MulticastApplyEdits(PendingEdits);
			PendingEdits.Reset();
		}
		// Late join: a client that connects after edits has no edit history, so
		// the authority pushes its current state. The burst (not a single send)
		// covers the channel race: PostLogin fires before the client holds this
		// actor's channel.
		if (UWorld* W = GetWorld())
		{
			if (UNetDriver* ND = W->GetNetDriver())
			{
				const int32 NumConns = ND->ClientConnections.Num();
				if (NumConns != LastConnectionCount)
				{
					LastConnectionCount = NumConns;
					// Small burst, not a stream: each reliable multicast carries the
					// whole 32KB chunk, so retries are bounded to stay well inside
					// the reliable buffer while still covering the channel race.
					SnapshotRetriesLeft = NumConns > 0 ? 3 : 0;
					NextSnapshotTime = W->GetTimeSeconds() + 0.5;
					UE_LOG(LogAfterfall, Log,
						TEXT("VOXELCHUNK connections=%d -> late-join snapshot burst"), NumConns);
				}
				if (SnapshotRetriesLeft > 0 && W->GetTimeSeconds() >= NextSnapshotTime)
				{
					--SnapshotRetriesLeft;
					NextSnapshotTime = W->GetTimeSeconds() + 2.5;
					ServerSendSnapshotToAll();
				}
			}
		}
	}
	else if (!bHasSnapshot && LastAppliedSeq == 0)
	{
		UWorld* W = GetWorld();
		if (W && W->GetTimeSeconds() > 1.5)
		{
			RequestResync();
		}
	}
	if (!HasAuthority() && bResyncPending)
	{
		RequestResync();
	}
	if (!HasAuthority() && !bLoggedClientTick)
	{
		bLoggedClientTick = true;
		UE_LOG(LogAfterfall, Log,
			TEXT("VOXELCLIENT chunk tick (netmode=%d snapshot=%d solid=%d)"),
			(int32)GetNetMode(), bHasSnapshot ? 1 : 0, VoxelData.CountSolid());
	}

	if (bMeshDirty)
	{
		FlushMeshRebuild();
	}
}

// ---------------------------------------------------------------- server ---

void AVoxelChunkActor::ServerInitializeChunk()
{
	if (!HasAuthority())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VOXELCHUNK init rejected (server only)"));
		return;
	}
	VoxelData.InitializeDeterministic();
	ServerSeq = 0;
	LastAppliedSeq = 0;
	PendingEdits.Reset();
	EditLog.Reset();
	MarkMeshDirty();
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCHUNK initialized solid=%d seq=%d"),
		VoxelData.CountSolid(), ServerSeq);
}

bool AVoxelChunkActor::ServerApplyVoxelEdit(const FIntVector& Local, uint8 State)
{
	if (!HasAuthority())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VOXELCHUNK edit rejected (not authority)"));
		return false;
	}
	if (!FVoxelChunkData::IsValidCoord(Local))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VOXELCHUNK edit rejected coord=(%d,%d,%d)"),
			Local.X, Local.Y, Local.Z);
		return false;
	}
	if (State > AfterfallVoxel::MaxMaterialId)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VOXELCHUNK edit rejected state=%d"), (int32)State);
		return false;
	}
	if (VoxelData.Get(Local) == State)
	{
		return false; // no-op: never burns a sequence number
	}
	VoxelData.Set(Local, State);
	++ServerSeq;

	FVoxelEdit Edit;
	Edit.Coord = Local;
	Edit.State = State;
	Edit.Seq = ServerSeq;
	EditLog.Add(Edit);
	if (EditLog.Num() > AfterfallVoxel::EditLogCap)
	{
		EditLog.RemoveAt(0);
	}
	PendingEdits.Add(Edit);
	MarkMeshDirty();
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCHUNK edit seq=%d coord=(%d,%d,%d) state=%d solid=%d"),
		ServerSeq, Local.X, Local.Y, Local.Z, (int32)State, VoxelData.CountSolid());
	return true;
}

int32 AVoxelChunkActor::ServerApplyVoxelEdits(const TArray<FVoxelEdit>& Edits)
{
	int32 Applied = 0;
	for (const FVoxelEdit& E : Edits)
	{
		// Client-supplied Seq is never trusted: the server assigns its own.
		if (ServerApplyVoxelEdit(E.Coord, E.State))
		{
			++Applied;
		}
	}
	return Applied;
}

bool AVoxelChunkActor::ServerApplyFractureImpact(const FVector& ImpactPos, const FVector& ImpactDir, float Energy, uint32 Seed, EMaterialType Material)
{
	if (!HasAuthority()) return false;
	FIntVector Start = FVoxelBasis::WorldToVoxel(ImpactPos - GetActorLocation(), FVector::ZeroVector);
	if (!FVoxelChunkData::IsValidCoord(Start)) return false;
	AfterfallFracture::FFractureResult R = AfterfallFracture::Solve(Start, Seed, Material, Energy, 8, ImpactDir);
	int32 Applied = 0;
	for (const FIntVector& C : R.Path)
	{
		if (!FVoxelChunkData::IsValidCoord(C)) continue;
		if (ServerApplyVoxelEdit(C, 0)) ++Applied;
	}
	// initiation point too
	if (FVoxelChunkData::IsValidCoord(Start))
	{
		ServerApplyVoxelEdit(Start, 0);
	}
	return Applied > 0;
}

void AVoxelChunkActor::ServerSendSnapshotToAll()
{
	if (!HasAuthority())
	{
		return;
	}
	FVoxelSnapshot Snapshot;
	Snapshot.Data = VoxelData;
	Snapshot.Seq = ServerSeq;
	MulticastResetChunk(Snapshot);
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCHUNK snapshot sent seq=%d solid=%d"),
		Snapshot.Seq, Snapshot.Data.CountSolid());
}

// ---------------------------------------------------------------- client ---

void AVoxelChunkActor::MulticastApplyEdits_Implementation(const TArray<FVoxelEdit>& Edits)
{
	if (HasAuthority())
	{
		return;
	}
	int32 Applied = 0;
	bool bGap = false;
	for (const FVoxelEdit& E : Edits)
	{
		if (AfterfallVoxel::IsGap(E.Seq, LastAppliedSeq))
		{
			bGap = true;
		}
		if (AfterfallVoxel::ApplyEdit(VoxelData, E, LastAppliedSeq))
		{
			++Applied;
		}
	}
	if (Applied > 0)
	{
		MarkMeshDirty();
	}
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCLIENT edits=%d applied=%d solid=%d seq=%d%s"),
		Edits.Num(), Applied, VoxelData.CountSolid(), LastAppliedSeq,
		bGap ? TEXT(" GAP") : TEXT(""));
	if (bGap)
	{
		RequestResync(); // hole in the stream: pull the full state
	}
}

void AVoxelChunkActor::MulticastResetChunk_Implementation(const FVoxelSnapshot& Snapshot)
{
	if (HasAuthority() || !Snapshot.Data.IsValid())
	{
		return;
	}
	if (Snapshot.Seq < LastAppliedSeq)
	{
		// Older than what incremental edits already produced: ignore, so a
		// delayed resync cannot roll the client backwards.
		return;
	}
	VoxelData = Snapshot.Data;
	LastAppliedSeq = Snapshot.Seq;
	bHasSnapshot = true;
	MarkMeshDirty();
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCLIENT snapshot applied seq=%d solid=%d"),
		LastAppliedSeq, VoxelData.CountSolid());
}

void AVoxelChunkActor::RequestResync()
{
	if (HasAuthority())
	{
		return;
	}
	UWorld* W = GetWorld();
	const double Now = W ? W->GetTimeSeconds() : 0.0;
	if (Now - LastResyncRequestTime < 3.0)
	{
		return;
	}
	LastResyncRequestTime = Now;
	// Server RPCs must travel over the player's own connection: this actor has
	// no owning connection, so the request goes through the local pawn.
	AAfterfallCharacter* Pawn = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(PC->GetPawn()))
			{
				Pawn = C;
				break;
			}
		}
	}
	if (!Pawn)
	{
		Pawn = Cast<AAfterfallCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	}
	if (Pawn)
	{
		Pawn->ServerRequestVoxelResync(this);
		bResyncPending = false;
		UE_LOG(LogAfterfall, Log, TEXT("VOXELCLIENT resync requested via %s"), *Pawn->GetName());
	}
	else
	{
		bResyncPending = true;
		static double LastWarn = -100.0;
		if (Now - LastWarn > 5.0)
		{
			LastWarn = Now;
			UE_LOG(LogAfterfall, Warning, TEXT("VOXELCLIENT resync request deferred: no local pawn yet"));
		}
	}
}

// ------------------------------------------------------------------ mesh ---

void AVoxelChunkActor::FlushMeshRebuild()
{
	bMeshDirty = false;
	if (!ChunkMesh)
	{
		return;
	}
	VoxelMesher::FMeshBuffer Buffer;
	VoxelMesher::BuildChunkMesh(VoxelData, Buffer);
	LastQuadCount = Buffer.QuadCount;
	++MeshBuildCount;

	if (Buffer.QuadCount == 0)
	{
		ChunkMesh->ClearAllMeshSections();
		return;
	}
	// One material section. Collision is rebuilt with the mesh (synchronous
	// cooking), so replicated edits are collision-correct immediately.
	TArray<FProcMeshTangent> Tangents; // default tangents are fine
	ChunkMesh->CreateMeshSection_LinearColor(0, Buffer.Positions, Buffer.Indices,
		Buffer.Normals, Buffer.UVs, Buffer.Colors, Tangents, /*bCreateCollision=*/true);
}
