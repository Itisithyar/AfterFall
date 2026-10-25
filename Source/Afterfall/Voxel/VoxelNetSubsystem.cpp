// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — headless two-process replication verification.
#include "Voxel/VoxelNetSubsystem.h"
#include "Voxel/VoxelChunkActor.h"
#include "Voxel/VoxelBasis.h"
#include "AfterfallCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Afterfall.h"

namespace
{
	// Deterministic scene used by both sides of the test.
	constexpr int32 InitSolid = 32 * 32 * 8;   // 8192
	constexpr int32 EditedSolid = InitSolid - 2 + 1; // remove 2, place 1 => 8191

	const FIntVector RemoveA(10, 10, 3);
	const FIntVector PlaceB(20, 20, 20);
	const FIntVector RemoveC(0, 0, 0);
	const FIntVector ClientEdit(5, 5, 5);
	const uint8 ClientEditState = 3; // metal
}

bool UVoxelNetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* W = Cast<UWorld>(Outer))
	{
		return W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE;
	}
	return false;
}

void UVoxelNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("afVoxelServer")))
	{
		Mode = EMode::Server;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("afVoxelClientVerify")))
	{
		Mode = EMode::Client;
	}
	if (Mode == EMode::None)
	{
		return;
	}
	if (UWorld* W = GetWorld())
	{
		UE_LOG(LogAfterfall, Log, TEXT("VOXELNET %s fixture started"),
			Mode == EMode::Server ? TEXT("server") : TEXT("client"));
		W->GetTimerManager().SetTimer(StepTimer, this, &UVoxelNetSubsystem::Step,
			0.25f, true, 2.0f);
	}
}

void UVoxelNetSubsystem::Step()
{
	if (Mode == EMode::Server)
	{
		StepServer();
	}
	else if (Mode == EMode::Client)
	{
		StepClient();
	}
}

AVoxelChunkActor* UVoxelNetSubsystem::FindChunk() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<AVoxelChunkActor> It(W); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

AAfterfallCharacter* UVoxelNetSubsystem::FindLocalPawn() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* P = PC->GetPawn())
			{
				if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(P))
				{
					return C;
				}
			}
		}
	}
	return nullptr;
}

bool UVoxelNetSubsystem::EditVoxel(FIntVector Local, uint8 State)
{
	AVoxelChunkActor* Chunk = FindChunk();
	if (!Chunk)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VOXELNET no chunk in world"));
		return false;
	}
	return Chunk->ServerApplyVoxelEdit(Local, State);
}

// ---------------------------------------------------------------- server ---

void UVoxelNetSubsystem::StepServer()
{
	if (Phase == 0)
	{
		UWorld* W = GetWorld();
		if (!W || W->IsNetMode(NM_Client))
		{
			UE_LOG(LogAfterfall, Warning, TEXT("VOXELSERVER requires a server world"));
			return;
		}
		AVoxelChunkActor* Chunk = FindChunk();
		if (!Chunk)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			Chunk = W->SpawnActor<AVoxelChunkActor>(AVoxelChunkActor::StaticClass(),
				FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		if (!Chunk)
		{
			UE_LOG(LogAfterfall, Error, TEXT("VOXELSERVER FAIL chunk spawn"));
			Phase = 9;
			return;
		}
		UE_LOG(LogAfterfall, Log, TEXT("VOXELSERVER chunk %s spawned (netmode=%d)"),
			*Chunk->GetName(), (int32)Chunk->GetNetMode());
		Chunk->ServerInitializeChunk();
		// Three authoritative edits while NO client is connected: a client that
		// joins afterwards must receive the edited world, not the pristine one.
		Chunk->ServerApplyVoxelEdit(RemoveA, 0);
		Chunk->ServerApplyVoxelEdit(PlaceB, 2);
		Chunk->ServerApplyVoxelEdit(RemoveC, 0);
		Chunk->FlushMeshRebuild();
		UE_LOG(LogAfterfall, Log,
			TEXT("VOXELSERVER chunk replicated=%d always=%d"),
			Chunk->GetIsReplicated() ? 1 : 0, Chunk->bAlwaysRelevant ? 1 : 0);
		UE_LOG(LogAfterfall, Log,
			TEXT("VOXELSERVER READY seq=%d solid=%d quads=%d expect solid=%d"),
			Chunk->GetServerSeq(), Chunk->GetVoxelData().CountSolid(),
			Chunk->GetLastQuadCount(), EditedSolid);
		Phase = 1;
		PhaseStart = GetWorld()->GetTimeSeconds();
		Polls = 0;
		return;
	}
	if (Phase == 1)
	{
		// Fixture scaffolding: /Engine/Maps/Entry has no PlayerStart, so a
		// connecting client would get no pawn and could not relay a voxel edit
		// request (server RPCs need an owning connection). Possess one here so
		// the client edit path is testable; gameplay maps are untouched.
		if (UWorld* W = GetWorld())
		{
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (!PC || Cast<AAfterfallCharacter>(PC->GetPawn()))
				{
					continue;
				}
				// Entry has no PlayerStart, so GameModeBase hands the client a
				// plain ADefaultPawn (or nothing). Replace it with our pawn so the
				// client can relay voxel edit requests over its own connection.
				if (APawn* Old = PC->GetPawn())
				{
					PC->UnPossess();
					Old->Destroy();
				}
				FActorSpawnParameters P;
				P.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AAfterfallCharacter* Pawn = W->SpawnActor<AAfterfallCharacter>(
					AAfterfallCharacter::StaticClass(), FVector(0.f, 0.f, 120.f),
					FRotator::ZeroRotator, P);
				if (Pawn)
				{
					PC->Possess(Pawn);
					UE_LOG(LogAfterfall, Log, TEXT("VOXELSERVER possessed %s for %s"),
						*Pawn->GetName(), *PC->GetName());
				}
			}
		}
		// Heartbeat: proves the authoritative chunk stays up and stable while
		// the client observes it.
		++Polls;
		if (Polls % 40 == 0) // every 10s
		{
			AVoxelChunkActor* Chunk = FindChunk();
			if (Chunk)
			{
				UE_LOG(LogAfterfall, Log,
					TEXT("VOXELSERVER heartbeat seq=%d solid=%d quads=%d builds=%d"),
					Chunk->GetServerSeq(), Chunk->GetVoxelData().CountSolid(),
					Chunk->GetLastQuadCount(), Chunk->GetMeshBuildCount());
			}
		}
	}
}

// ---------------------------------------------------------------- client ---

void UVoxelNetSubsystem::Check(bool bOk, const TCHAR* Name, const FString& Detail)
{
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCLIENT %s %s (%s)"),
		bOk ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
	if (bOk)
	{
		++Pass;
	}
	else
	{
		++Fail;
	}
}

void UVoxelNetSubsystem::StepClient()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	if (W->IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	AVoxelChunkActor* Chunk = FindChunk();
	const double Now = W->GetTimeSeconds();

	switch (Phase)
	{
	case 0: // Late-join convergence: chunk actor replicated, edited state applied.
	{
		if (!Chunk)
		{
			++LocalIdlePoll;
			if (LocalIdlePoll > 240) // 60s
			{
				Check(false, TEXT("chunk replicated"), TEXT("no AVoxelChunkActor after 60s"));
				FinishClient();
			}
			return;
		}
		if (Polls == 0)
		{
			UE_LOG(LogAfterfall, Log,
				TEXT("VOXELCLIENT chunk found name=%s authority=%d role=%d remote=%d seq=%d solid=%d"),
				*Chunk->GetName(), Chunk->HasAuthority() ? 1 : 0, (int32)Chunk->GetLocalRole(),
				(int32)Chunk->GetRemoteRole(), Chunk->GetServerSeq(),
				Chunk->GetVoxelData().CountSolid());
		}
		const int32 Seq = Chunk->GetLastAppliedSeq();
		const int32 Solid = Chunk->GetVoxelData().CountSolid();
		if (Seq >= 3 && Solid == EditedSolid)
		{
			Check(true, TEXT("late join snapshot"),
				FString::Printf(TEXT("seq=%d solid=%d (expect %d)"), Seq, Solid, EditedSolid));
			Check(Chunk->GetVoxelData().Get(RemoveA) == 0, TEXT("removed voxel empty"),
				FString::Printf(TEXT("state=%d"), (int32)Chunk->GetVoxelData().Get(RemoveA)));
			Check(Chunk->GetVoxelData().Get(PlaceB) == 2, TEXT("placed voxel wood"),
				FString::Printf(TEXT("state=%d"), (int32)Chunk->GetVoxelData().Get(PlaceB)));
			Phase = 1;
			Polls = 0;
			PhaseStart = Now;
			return;
		}
		++Polls;
		if (Polls > 240)
		{
			Check(false, TEXT("late join snapshot"),
				FString::Printf(TEXT("timeout seq=%d solid=%d expect %d"), Seq, Solid, EditedSolid));
			FinishClient();
		}
		return;
	}
	case 1: // Mesh + collision were rebuilt client-side from replicated data.
	{
		Chunk->FlushMeshRebuild();
		Check(Chunk->GetLastQuadCount() > 0, TEXT("client mesh rebuilt"),
			FString::Printf(TEXT("quads=%d builds=%d"), Chunk->GetLastQuadCount(), Chunk->GetMeshBuildCount()));
		// Trace down through the slab: complex-as-simple collision must block.
		const FVector ProbeLocal = FVoxelBasis::VoxelToWorld(FIntVector(10, 10, 8), FVector::ZeroVector);
		const FVector Start = Chunk->GetActorLocation() + ProbeLocal + FVector(0.f, 0.f, 200.f);
		const FVector End = Start - FVector(0.f, 0.f, 400.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFVoxelClientProbe), false, nullptr);
		const bool bHit = W->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		Check(bHit && Hit.GetActor() == Chunk, TEXT("client collision blocks"),
			bHit ? FString::Printf(TEXT("hit %s at Z=%.1f"), *GetNameSafe(Hit.GetActor()), Hit.ImpactPoint.Z)
			     : FString(TEXT("no hit")));
		// Invalid (out-of-chunk) edit request: must be ignored, not crash.
		if (AAfterfallCharacter* Pawn = FindLocalPawn())
		{
			Pawn->ServerRequestVoxelEdit(Chunk, FIntVector(99, 0, 0), 1);
			Check(true, TEXT("invalid edit sent"), TEXT("coord=(99,0,0) expected rejected"));
		}
		else
		{
			Check(false, TEXT("local pawn"), TEXT("no AAfterfallCharacter to relay the request"));
			FinishClient();
			return;
		}
		Phase = 2;
		PhaseStart = Now;
		Polls = 0;
		return;
	}
	case 2: // Rejection window: authoritative seq must not move.
	{
		if (Now - PhaseStart < 2.5)
		{
			return;
		}
		const int32 Seq = Chunk->GetLastAppliedSeq();
		Check(Seq == 3, TEXT("invalid edit rejected"),
			FString::Printf(TEXT("client seq=%d (expect 3, unchanged)"), Seq));
		if (AAfterfallCharacter* Pawn = FindLocalPawn())
		{
			Pawn->ServerRequestVoxelEdit(Chunk, PlaceB, 0);
			Pawn->ServerRequestVoxelEdit(Chunk, ClientEdit, ClientEditState);
		}
		Phase = 3;
		PhaseStart = Now;
		Polls = 0;
		return;
	}
	case 3: // Incremental edit after join: server commits, client converges.
	{
		const int32 Seq = Chunk->GetLastAppliedSeq();
		const uint8 St = Chunk->GetVoxelData().Get(ClientEdit);
		if (Seq >= 4 && St == ClientEditState)
		{
			Check(true, TEXT("client edit round trip"),
				FString::Printf(TEXT("seq=%d state=%d solid=%d"), Seq, (int32)St,
					Chunk->GetVoxelData().CountSolid()));
			Check(Chunk->GetLastAppliedSeq() >= 4, TEXT("authority seq advanced"),
				FString::Printf(TEXT("seq=%d"), Chunk->GetLastAppliedSeq()));
			FinishClient();
			return;
		}
		if (Now - PhaseStart > 20.0)
		{
			Check(false, TEXT("client edit round trip"),
				FString::Printf(TEXT("timeout seq=%d state=%d"), Seq, (int32)St));
			FinishClient();
			return;
		}
		return;
	}
	default:
		return;
	}
}

void UVoxelNetSubsystem::FinishClient()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StepTimer);
	}
	const bool bOk = Fail == 0;
	UE_LOG(LogAfterfall, Log, TEXT("VOXELCLIENT SUMMARY pass=%d fail=%d VOXELCLIENT %s"),
		Pass, Fail, bOk ? TEXT("PASS") : TEXT("FAIL"));
	Mode = EMode::None;
	FPlatformMisc::RequestExitWithStatus(false, bOk ? 0 : 1);
}

// -------------------------------------------------------------- console ---

static FAutoConsoleCommandWithWorldAndArgs VoxelEditCmd(
	TEXT("afVoxelEdit"),
	TEXT("AFTERFALL voxel: afVoxelEdit X Y Z State (server-authoritative chunk edit)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* W)
		{
			if (Args.Num() < 4)
			{
				UE_LOG(LogAfterfall, Warning, TEXT("afVoxelEdit needs X Y Z State"));
				return;
			}
			const FIntVector Local(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]),
				FCString::Atoi(*Args[2]));
			const uint8 State = (uint8)FMath::Clamp(FCString::Atoi(*Args[3]), 0, 255);
			if (!W)
			{
				return;
			}
			if (UVoxelNetSubsystem* S = W->GetSubsystem<UVoxelNetSubsystem>())
			{
				UE_LOG(LogAfterfall, Log, TEXT("afVoxelEdit (%d,%d,%d)=%d -> %s"),
					Local.X, Local.Y, Local.Z, (int32)State,
					S->EditVoxel(Local, State) ? TEXT("committed") : TEXT("rejected/no-op"));
			}
		}));

static FAutoConsoleCommandWithWorld VoxelResyncCmd(
	TEXT("afVoxelResync"),
	TEXT("AFTERFALL voxel: push a full chunk snapshot to every client"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		for (TActorIterator<AVoxelChunkActor> It(W); It; ++It)
		{
			It->ServerSendSnapshotToAll();
		}
	}));

static FAutoConsoleCommandWithWorld VoxelDumpCmd(
	TEXT("afVoxelDump"),
	TEXT("AFTERFALL voxel: log chunk authority/instrumentation state"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		for (TActorIterator<AVoxelChunkActor> It(W); It; ++It)
		{
			UE_LOG(LogAfterfall, Log,
				TEXT("afVoxelDump authority=%d serverSeq=%d appliedSeq=%d solid=%d quads=%d builds=%d"),
				It->HasAuthority() ? 1 : 0, It->GetServerSeq(), It->GetLastAppliedSeq(),
				It->GetVoxelData().CountSolid(), It->GetLastQuadCount(), It->GetMeshBuildCount());
		}
	}));
