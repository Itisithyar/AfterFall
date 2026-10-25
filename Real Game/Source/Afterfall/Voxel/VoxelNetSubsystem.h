// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — reproducible two-process verification for the
// single-chunk replication proof, plus console tooling.
//
//   UnrealEditor.exe RealGame.uproject /Engine/Maps/Entry -server \
//       -afVoxelServer            spawns + initialises the chunk, applies three
//                                 authoritative edits BEFORE any client joins,
//                                 logs VOXELSERVER READY, stays up.
//   UnrealEditor.exe RealGame.uproject 127.0.0.1 -game \
//       -afVoxelClientVerify      joins late, asserts the edited state converged,
//                                 checks mesh + collision, tries an invalid edit
//                                 (must be ignored) and a valid one (must round
//                                 trip), logs VOXELCLIENT SUMMARY and exits.
//
// Console: afVoxelEdit X Y Z State (server), afVoxelResync, afVoxelDump.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelNetSubsystem.generated.h"

class AVoxelChunkActor;
class AAfterfallCharacter;

UCLASS()
class AFTERFALL_API UVoxelNetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Run one authoritative edit (server only). Returns false if rejected. */
	bool EditVoxel(FIntVector Local, uint8 State);

private:
	enum class EMode : uint8 { None, Server, Client };

	FTimerHandle StepTimer;
	EMode Mode = EMode::None;
	int32 Phase = 0;
	int32 Polls = 0;
	int32 Pass = 0;
	int32 Fail = 0;
	int32 LocalIdlePoll = 0;
	double PhaseStart = 0.0;

	void Step();
	void StepServer();
	void StepClient();
	void Check(bool bOk, const TCHAR* Name, const FString& Detail);
	void FinishClient();

	AVoxelChunkActor* FindChunk() const;
	AAfterfallCharacter* FindLocalPawn() const;
};
