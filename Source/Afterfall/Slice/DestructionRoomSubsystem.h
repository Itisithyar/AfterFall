// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Slice) — dev-only destructible room fixture. Map-agnostic
// like the slice: builds one floor + four walls (west wall yawed 90°) at
// runtime, then drives melee + hitscan impacts through the REAL server path
// (ServerApplyImpact -> propagation -> commit -> replication -> ISMC rebuild),
// verifies collision with real line traces, and round-trips ExportDelta ->
// destroy -> respawn -> ImportDelta. Commands: afBuildRoom, afRoomSelfTest,
// afRoomServer, afRoomClientVerify (replication check shares this code).
// This is a test fixture, not a playable map.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Structural/StructuralTypes.h"
#include "Destruction/DestructionPropagation.h"
#include "Persistence/RegionStateSave.h"
#include "DestructionRoomSubsystem.generated.h"

class AStructuralActor;

UCLASS()
class AFTERFALL_API UDestructionRoomSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Idempotent: builds the room once (floor + N/S/E/W walls, W yawed 90°). */
	void BuildRoom();

	/** Scripted end-to-end test. Server only. Ends with a quit in -game. */
	void RunRoomSelfTest();

	/** Replication server: builds the room, applies melee+hitscan+rotated
	 * impacts through the server path, logs ROOMSERVER READY, and stays up
	 * so a late-joining client can observe the replicated state. Server only. */
	void RunRoomServer();

	/** Replication client: polls for the server's room actors, asserts the
	 * replicated detached state, visuals, collision, and that client-side
	 * commits are rejected, then quits. Client only. Safe to start before
	 * the network join completes (auto-starts on the post-travel world). */
	void RunRoomClientVerify();

	/** Live multiclient: same assertions as RunRoomClientVerify, but first
	 * requires an intact baseline (proves the client was connected BEFORE
	 * destruction), then waits for the live transition and only then
	 * checks. A first sight that is already damaged FAILs loudly instead
	 * of passing falsely. Client only. */
	void RunRoomLiveClient();

private:
	// Roles into Room[]: 0 floor, 1 north, 2 south, 3 east, 4 west (rotated).
	TArray<TWeakObjectPtr<AStructuralActor>> Room;
	FTimerHandle TestTimer;
	FTimerHandle ClientTimer;
	int32 TestStep = 0;
	int32 PassCount = 0;
	int32 FailCount = 0;
	TArray<FStructureDelta> SavedDeltas;
	// Client-verify state (persists across polls within one world).
	int32 ClientPolls = 0;
	int32 ClientPass = 0;
	int32 ClientFail = 0;
	int32 ClientNorthBefore = INDEX_NONE;
	int32 ClientSettledPolls = 0;
	bool bClientBypassSent = false;
	bool bClientCoreChecked = false;
	// Live-multiclient mode (tag switches ROOMCLIENT log markers to ROOMLIVE).
	const TCHAR* ClientTag = TEXT("ROOMCLIENT");
	bool bLiveMode = false;
	bool bBaselineSeen = false;
	int32 LiveWaitPolls = 0;
	// Server wait-for-clients state (live path only).
	FTimerHandle ServerWaitTimer;
	int32 ServerWaitTarget = 0;
	int32 ServerStablePolls = 0;
	int32 ServerWaitPolls = 0;

	bool RoomIsValid() const;
	AStructuralActor* SpawnRoomPart(EStructuralPart Part, const FVector& At, const FRotator& Rot, float ExtentCm);
	void Check(bool bOk, const TCHAR* Name, const FString& Detail);
	int32 ApplyLocalHit(AStructuralActor* Wall, const FVector& LocalOrigin, const FVector& LocalDir,
		float Energy, EDestructionInitiator Initiator, float DamagePerCell);
	/** Traces through one grid cell. Returns hit actor (nullptr = nothing hit). */
	AActor* TraceThroughCell(AStructuralActor* Wall, int32 CellIndex, const FVector& LocalDir,
		FString& OutDetail);
	void StepSelfTest();
	void FinishSelfTest();
	void StartClientVerify(const TCHAR* Tag, bool bLive);
	void StepClientVerify();
	void FinishClientVerify();
	void StepRoomServerWait();
	void StepRoomServerHeartbeat();
	void ApplyRoomImpacts(int32& OutMelee, int32& OutHitscan, int32& OutRotated);
	int32 CountOpenConnections() const;
	void ClientCheck(bool bOk, const TCHAR* Name, const FString& Detail);
	AStructuralActor* FindClientPart(const TArray<AStructuralActor*>& Actors, const FVector& At,
		float YawDeg) const;
};
