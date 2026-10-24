// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "DestructionRoomSubsystem.h"
#include "Structural/StructuralActor.h"
#include "Structural/ConstructionValidator.h"
#include "Structural/StructuralDestructionComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Afterfall.h"

namespace
{
constexpr float RoomX = 2000.f;
constexpr float WallZ = 490.f;
constexpr float WallExtent = 100.f;
constexpr float FloorExtent = 200.f;

void RoomLog(bool bOk, const TCHAR* Name, const FString& Detail)
{
	UE_LOG(LogAfterfall, Log, TEXT("ROOMTEST %s %s (%s)"),
		bOk ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
}
} // namespace

bool UDestructionRoomSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* W = Cast<UWorld>(Outer))
	{
		return true;
	}
	return false;
}

void UDestructionRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAfterfall, Log, TEXT("ROOMTEST subsystem init, cmd=%s"), FCommandLine::Get());
	if (FParse::Param(FCommandLine::Get(), TEXT("afRoomSelfTest")))
	{
		if (UWorld* W = GetWorld())
		{
			FTimerHandle H;
			W->GetTimerManager().SetTimer(H, this, &UDestructionRoomSubsystem::RunRoomSelfTest, 1.0f, false);
		}
	}
	// Replication path: server builds + impacts and stays up; the client
	// verify self-guards on net mode so the post-travel world picks it up.
	if (FParse::Param(FCommandLine::Get(), TEXT("afRoomServer")))
	{
		if (UWorld* W = GetWorld())
		{
			FTimerHandle H;
			W->GetTimerManager().SetTimer(H, this, &UDestructionRoomSubsystem::RunRoomServer, 1.0f, false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("afRoomClientVerify")))
	{
		RunRoomClientVerify();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("afRoomLiveClient")))
	{
		RunRoomLiveClient();
	}
}

void UDestructionRoomSubsystem::Deinitialize()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(TestTimer);
		W->GetTimerManager().ClearTimer(ClientTimer);
		W->GetTimerManager().ClearTimer(ServerWaitTimer);
	}
	Super::Deinitialize();
}

bool UDestructionRoomSubsystem::RoomIsValid() const
{
	if (Room.Num() != 5)
	{
		return false;
	}
	for (const TWeakObjectPtr<AStructuralActor>& A : Room)
	{
		if (!A.IsValid())
		{
			return false;
		}
	}
	return true;
}

AStructuralActor* UDestructionRoomSubsystem::SpawnRoomPart(EStructuralPart Part, const FVector& At,
	const FRotator& Rot, float ExtentCm)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AStructuralActor* A = W->SpawnActor<AStructuralActor>(AStructuralActor::StaticClass(), At, Rot, P);
	if (!A || !A->Destruction)
	{
		return nullptr;
	}
	A->Destruction->CellExtentCm = ExtentCm;
	FStructuralSpec Spec;
	Spec.Part = Part;
	Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Part);
	Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Part);
	A->ServerInitialize(Spec, 0);
	A->RefreshDestructionVisual();
	return A;
}

void UDestructionRoomSubsystem::BuildRoom()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ROOMTEST afBuildRoom requires server"));
		return;
	}
	if (RoomIsValid())
	{
		UE_LOG(LogAfterfall, Log, TEXT("ROOMTEST room already built"));
		return;
	}
	Room.Reset();
	// Roles: 0 floor, 1 north, 2 south, 3 east, 4 west (yawed 90°, doorway gap at +X side).
	Room.Add(SpawnRoomPart(EStructuralPart::Floor, FVector(RoomX, 0.f, 100.f), FRotator::ZeroRotator, FloorExtent));
	Room.Add(SpawnRoomPart(EStructuralPart::Wall, FVector(RoomX, 300.f, WallZ), FRotator::ZeroRotator, WallExtent));
	Room.Add(SpawnRoomPart(EStructuralPart::Wall, FVector(RoomX, -300.f, WallZ), FRotator::ZeroRotator, WallExtent));
	Room.Add(SpawnRoomPart(EStructuralPart::Wall, FVector(RoomX + 300.f, 0.f, WallZ), FRotator::ZeroRotator, WallExtent));
	Room.Add(SpawnRoomPart(EStructuralPart::Wall, FVector(RoomX - 300.f, -150.f, WallZ), FRotator(0.f, 90.f, 0.f), WallExtent));
	UE_LOG(LogAfterfall, Log, TEXT("ROOMTEST room built: %d/5 live"), Room.Num());
}

void UDestructionRoomSubsystem::Check(bool bOk, const TCHAR* Name, const FString& Detail)
{
	RoomLog(bOk, Name, Detail);
	if (bOk)
	{
		++PassCount;
	}
	else
	{
		++FailCount;
	}
}

int32 UDestructionRoomSubsystem::ApplyLocalHit(AStructuralActor* Wall, const FVector& LocalOrigin,
	const FVector& LocalDir, float Energy, EDestructionInitiator Initiator, float DamagePerCell)
{
	const FTransform T = Wall->GetActorTransform();
	FDestructionEvent E;
	E.Origin = T.TransformPosition(LocalOrigin);
	E.Direction = T.TransformVector(LocalDir);
	E.InitialEnergy = Energy;
	E.Initiator = Initiator;
	E.MaterialId = UDestructionPropagationService::MaterialForPart(Wall->Spec.Part);
	return Wall->ServerApplyImpact(E, DamagePerCell, FGuid::NewGuid());
}

AActor* UDestructionRoomSubsystem::TraceThroughCell(AStructuralActor* Wall, int32 CellIndex,
	const FVector& LocalDir, FString& OutDetail)
{
	UWorld* W = GetWorld();
	const FVector Local = UDestructionPropagationService::GridPoint(
		FVector::ZeroVector, Wall->Destruction->CellExtentCm,
		CellIndex % 3, (CellIndex / 3) % 3, CellIndex / 9);
	const FVector World = Wall->GetActorTransform().TransformPosition(Local);
	const FVector DirW = Wall->GetActorTransform().TransformVector(LocalDir).GetSafeNormal();
	FHitResult Hit;
	// NOTE: the wall under test is NOT ignored — otherwise an intact cell could
	// never report a hit and the hole check would pass trivially (false positive).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFRoomTrace), false);
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(W, 0))
	{
		Params.AddIgnoredActor(Pawn);
	}
	// Short segment centred on the cell: local probe, no unrelated geometry.
	W->LineTraceSingleByChannel(Hit, World - DirW * 25.f, World + DirW * 25.f, ECC_Visibility, Params);
	OutDetail = FString::Printf(TEXT("cell %d -> %s"), CellIndex,
		Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nothing"));
	return Hit.GetActor();
}

void UDestructionRoomSubsystem::RunRoomSelfTest()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ROOMTEST afRoomSelfTest requires server"));
		return;
	}
	BuildRoom();
	if (!RoomIsValid())
	{
		UE_LOG(LogAfterfall, Error, TEXT("ROOMTEST FAIL setup (room incomplete)"));
		return;
	}
	// Park the player inside the room so traces/presence are representative.
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(W, 0))
	{
		Pawn->SetActorLocation(FVector(RoomX, 0.f, WallZ));
	}
	TestStep = 0;
	PassCount = 0;
	FailCount = 0;
	SavedDeltas.Reset();
	W->GetTimerManager().SetTimer(TestTimer, this,
		&UDestructionRoomSubsystem::StepSelfTest, 0.6f, true);
}

void UDestructionRoomSubsystem::StepSelfTest()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	// The save step intentionally unloads every actor; the reload step rebuilds
	// from deltas, so only the other steps require a live room.
	const int32 ReloadStep = 6;
	if (TestStep != ReloadStep && !RoomIsValid())
	{
		W->GetTimerManager().ClearTimer(TestTimer);
		UE_LOG(LogAfterfall, Error, TEXT("ROOMTEST FAIL setup (room lost mid-test, step %d)"), TestStep);
		return;
	}
	// NOTE: no room lookups here — steps 5+ intentionally run with an empty
	// Room (post-unload / mid-reload). Each case fetches what it needs.
	switch (TestStep)
	{
	case 0:
	{
		AStructuralActor* North = Room[1].Get();
		// Melee: room-side face of the north wall, attack proxy.
		const int32 Added = ApplyLocalHit(North, FVector(0.f, -(WallExtent + 10.f), 0.f),
			FVector(0.f, 1.f, 0.f), UDestructionPropagationService::AttackEnergy(),
			EDestructionInitiator::Attack, UDestructionPropagationService::AttackDamagePerCell());
		Check(Added > 0, TEXT("melee detaches"), FString::Printf(TEXT("north +%d cells"), Added));
		break;
	}
	case 1:
	{
		AStructuralActor* East = Room[3].Get();
		// Hitscan: room-side face of the east wall, bullet proxy.
		const int32 Added = ApplyLocalHit(East, FVector(-(WallExtent + 10.f), 0.f, 0.f),
			FVector(1.f, 0.f, 0.f), UDestructionPropagationService::BulletEnergy(),
			EDestructionInitiator::Bullet, UDestructionPropagationService::BulletDamagePerCell());
		Check(Added > 0, TEXT("bullet detaches"), FString::Printf(TEXT("east +%d cells"), Added));
		break;
	}
	case 2:
	{
		AStructuralActor* North = Room[1].Get();
		AStructuralActor* West = Room[4].Get();
		// Rotated wall, SAME local hit as the north melee: local indices must match.
		const int32 Added = ApplyLocalHit(West, FVector(0.f, -(WallExtent + 10.f), 0.f),
			FVector(0.f, 1.f, 0.f), UDestructionPropagationService::AttackEnergy(),
			EDestructionInitiator::Attack, UDestructionPropagationService::AttackDamagePerCell());
		const TArray<int32>& N = North->DetachedCells;
		const TArray<int32>& Wc = West->DetachedCells;
		bool bSame = Added > 0 && N.Num() == Wc.Num();
		for (int32 i = 0; bSame && i < N.Num(); ++i)
		{
			bSame = N[i] == Wc[i];
		}
		Check(bSame, TEXT("rotated wall matches local pattern"),
			FString::Printf(TEXT("west +%d, north %d cells"), Added, N.Num()));
		break;
	}
	case 3:
	{
		AStructuralActor* North = Room[1].Get();
		// Collision: the wall itself is NOT ignored, so a detached cell must yield
		// no hit (empty space where the cube was) and an intact cell must hit it.
		const int32 Det = North->DetachedCells[0];
		FString HoleDetail;
		AActor* ThroughHole = TraceThroughCell(North, Det, FVector(0.f, 1.f, 0.f), HoleDetail);
		Check(ThroughHole == nullptr, TEXT("hole has no collision"), HoleDetail);
		int32 Intact = INDEX_NONE;
		for (int32 i = 0; i < 27; ++i)
		{
			if (!North->DetachedCells.Contains(i))
			{
				Intact = i;
				break;
			}
		}
		FString SolidDetail;
		AActor* OnSolid = TraceThroughCell(North, Intact, FVector(0.f, 1.f, 0.f), SolidDetail);
		Check(Intact != INDEX_NONE && OnSolid == North, TEXT("intact cell blocks"), SolidDetail);
		break;
	}
	case 4:
	{
		AStructuralActor* North = Room[1].Get();
		// Rebuild timing: intact floor, partial north wall, repeat idempotent rebuild.
		auto TimeRebuild = [&](AStructuralActor* A)
		{
			const double T0 = FPlatformTime::Seconds();
			A->Destruction->Rebuild(A->DetachedCells);
			return (FPlatformTime::Seconds() - T0) * 1000.0;
		};
		const double MsFloor = TimeRebuild(Room[0].Get());
		const double MsNorth = TimeRebuild(North);
		const double MsRepeat = TimeRebuild(North);
		const bool bSync = North->Destruction->IsInSync(North->DetachedCells);
		Check(bSync, TEXT("rebuild converges"),
			FString::Printf(TEXT("floor %.2fms north %.2fms repeat %.2fms instances %d"),
				MsFloor, MsNorth, MsRepeat, North->Destruction->VisibleCellCount()));
		break;
	}
	case 5:
	{
		// Save: export every room delta (the real persistence structs).
		for (const TWeakObjectPtr<AStructuralActor>& A : Room)
		{
			SavedDeltas.Add(A->ExportDelta());
		}
		Check(SavedDeltas.Num() == 5, TEXT("export deltas"), FString::Printf(TEXT("%d deltas"), SavedDeltas.Num()));
		// Unload: destroy every live representation (roots live on in the deltas).
		for (const TWeakObjectPtr<AStructuralActor>& A : Room)
		{
			if (A.IsValid())
			{
				A->Destroy();
			}
		}
		Room.Reset();
		break;
	}
	case 6:
	{
		// Reload: fresh actors, restore from deltas (also covers legacy: delta[0]
		// is re-imported with detached fields cleared to mimic an old save).
		bool bAllOk = true;
		for (int32 i = 0; i < SavedDeltas.Num(); ++i)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AStructuralActor* Fresh = W->SpawnActor<AStructuralActor>(
				AStructuralActor::StaticClass(), SavedDeltas[i].Location, SavedDeltas[i].Rotation, P);
			if (!Fresh || !Fresh->Destruction)
			{
				bAllOk = false;
				continue;
			}
			FStructureDelta D = SavedDeltas[i];
			if (i == 0)
			{
				D.DetachedCells.Reset(); // legacy save: damage fields absent
			}
			Fresh->ImportDelta(D);
			Room.Add(Fresh);
			const bool bCells = (i == 0)
				? (Fresh->DetachedCells.Num() == 0)
				: (Fresh->DetachedCells.Num() == SavedDeltas[i].DetachedCells.Num());
			const bool bGuid = Fresh->StableGuid == SavedDeltas[i].Guid;
			const bool bDur = FMath::IsNearlyEqual(Fresh->Durability, SavedDeltas[i].Durability, 0.01f);
			const bool bVis = Fresh->Destruction->VisibleCellCount() == 27 - Fresh->DetachedCells.Num();
			if (!(bCells && bGuid && bDur && bVis && Fresh->Destruction->IsInSync(Fresh->DetachedCells)))
			{
				bAllOk = false;
			}
		}
		Check(bAllOk && RoomIsValid(), TEXT("reload restores state"),
			FString::Printf(TEXT("%d/5 actors ok"), Room.Num()));
		break;
	}
	case 7:
	{
		// Reloaded collision still agrees with restored authority.
		AStructuralActor* RN = Room[1].Get();
		const int32 Det = RN->DetachedCells[0];
		FString ReloadDetail;
		AActor* ThroughHole = TraceThroughCell(RN, Det, FVector(0.f, 1.f, 0.f), ReloadDetail);
		Check(ThroughHole == nullptr, TEXT("reloaded hole has no collision"), ReloadDetail);
		break;
	}
	default:
		FinishSelfTest();
		return;
	}
	++TestStep;
}

void UDestructionRoomSubsystem::FinishSelfTest()
{
	UWorld* W = GetWorld();
	if (W)
	{
		W->GetTimerManager().ClearTimer(TestTimer);
	}
	UE_LOG(LogAfterfall, Log, TEXT("ROOMTEST SUMMARY pass=%d fail=%d %s"),
		PassCount, FailCount, FailCount == 0 ? TEXT("ROOMTEST PASS") : TEXT("ROOMTEST FAIL"));
	// Headless runtimes (standalone -game, editor-cmd server) have no player
	// controller, and console "quit" is ignored there — request exit directly.
	if (UWorld* W2 = GetWorld())
	{
		if (APlayerController* PC = W2->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("quit"), true);
		}
	}
	FPlatformMisc::RequestExit(0);
}

void UDestructionRoomSubsystem::RunRoomServer()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ROOMSERVER afRoomServer requires server"));
		return;
	}
	BuildRoom();
	if (!RoomIsValid())
	{
		UE_LOG(LogAfterfall, Error, TEXT("ROOMSERVER FAIL setup (room incomplete)"));
		return;
	}
	int32 WaitTarget = 0;
	FParse::Value(FCommandLine::Get(), TEXT("afRoomWaitClients="), WaitTarget);
	if (WaitTarget > 0)
	{
		// Live-multiclient path: room is built but undamaged; impacts fire
		// only after WaitTarget clients hold stable connections (plus a
		// settle window so both observe an intact baseline first).
		ServerWaitTarget = WaitTarget;
		ServerStablePolls = 0;
		ServerWaitPolls = 0;
		UE_LOG(LogAfterfall, Log,
			TEXT("ROOMSERVER waiting for %d clients (room built, undamaged)"), ServerWaitTarget);
		W->GetTimerManager().SetTimer(ServerWaitTimer, this,
			&UDestructionRoomSubsystem::StepRoomServerWait, 0.5f, true);
		return;
	}
	// Legacy path (late-join test): impact immediately, then stay up.
	int32 N0 = 0, E0 = 0, W0 = 0;
	ApplyRoomImpacts(N0, E0, W0);
	AStructuralActor* North = Room[1].Get();
	AStructuralActor* East = Room[3].Get();
	AStructuralActor* West = Room[4].Get();
	UE_LOG(LogAfterfall, Log,
		TEXT("ROOMSERVER READY north=%d east=%d west=%d floor=%d south=%d (new +%d/+%d/+%d)"),
		North->DetachedCells.Num(), East->DetachedCells.Num(), West->DetachedCells.Num(),
		Room[0].Get()->DetachedCells.Num(), Room[2].Get()->DetachedCells.Num(), N0, E0, W0);
	// Intentionally stays up: the late-joining client observes this state.
}

void UDestructionRoomSubsystem::ApplyRoomImpacts(int32& OutMelee, int32& OutHitscan, int32& OutRotated)
{
	// The single authoritative impact sequence, shared by every server path
	// (self-test steps 0-2, late-join server, live-multiclient server).
	// Idempotent: undamaged walls only, so re-runs stay READY.
	OutMelee = 0;
	OutHitscan = 0;
	OutRotated = 0;
	AStructuralActor* North = Room[1].Get();
	AStructuralActor* East = Room[3].Get();
	AStructuralActor* West = Room[4].Get();
	if (North && North->DetachedCells.Num() == 0)
	{
		OutMelee = ApplyLocalHit(North, FVector(0.f, -(WallExtent + 10.f), 0.f),
			FVector(0.f, 1.f, 0.f), UDestructionPropagationService::AttackEnergy(),
			EDestructionInitiator::Attack, UDestructionPropagationService::AttackDamagePerCell());
	}
	if (East && East->DetachedCells.Num() == 0)
	{
		OutHitscan = ApplyLocalHit(East, FVector(-(WallExtent + 10.f), 0.f, 0.f),
			FVector(1.f, 0.f, 0.f), UDestructionPropagationService::BulletEnergy(),
			EDestructionInitiator::Bullet, UDestructionPropagationService::BulletDamagePerCell());
	}
	if (West && West->DetachedCells.Num() == 0)
	{
		OutRotated = ApplyLocalHit(West, FVector(0.f, -(WallExtent + 10.f), 0.f),
			FVector(0.f, 1.f, 0.f), UDestructionPropagationService::AttackEnergy(),
			EDestructionInitiator::Attack, UDestructionPropagationService::AttackDamagePerCell());
	}
}

void UDestructionRoomSubsystem::StepRoomServerHeartbeat()
{
	const UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	int32 Valid = 0;
	for (const TWeakObjectPtr<AStructuralActor>& A : Room)
	{
		if (A.IsValid())
		{
			++Valid;
		}
	}
	UE_LOG(LogAfterfall, Log, TEXT("ROOMSERVER heartbeat (room %d/5 valid, conns=%d, north=%d)"),
		Valid, CountOpenConnections(),
		Room.IsValidIndex(1) && Room[1].IsValid() ? Room[1]->DetachedCells.Num() : -1);
}

int32 UDestructionRoomSubsystem::CountOpenConnections() const
{
	const UWorld* W = GetWorld();
	if (!W || !W->GetNetDriver())
	{
		return 0;
	}
	int32 N = 0;
	// NOTE: UNetConnection::State is private; count live entries instead.
	// Transient (pending/closing) entries settle in ~1s, while the caller
	// requires 10s of stability, so this cannot flap a full count.
	for (const UNetConnection* C : W->GetNetDriver()->ClientConnections)
	{
		if (C)
		{
			++N;
		}
	}
	return N;
}

void UDestructionRoomSubsystem::StepRoomServerWait()
{
	UWorld* W = GetWorld();
	if (!W || !ServerWaitTimer.IsValid())
	{
		return;
	}
	++ServerWaitPolls;
	const int32 Open = CountOpenConnections();
	if (Open >= ServerWaitTarget)
	{
		++ServerStablePolls;
	}
	else
	{
		ServerStablePolls = 0;
	}
	if (ServerWaitPolls % 20 == 0)
	{
		UE_LOG(LogAfterfall, Log, TEXT("ROOMSERVER waiting for clients (%d/%d open, stable %d/20)"),
			Open, ServerWaitTarget, ServerStablePolls);
	}
	// 20 stable polls = 10s with both clients connected: both have had time
	// to replicate the intact baseline before destruction fires.
	if (ServerStablePolls >= 20)
	{
		W->GetTimerManager().ClearTimer(ServerWaitTimer);
		ServerWaitTimer.Invalidate();
		int32 LN = 0, LE = 0, LW = 0;
		ApplyRoomImpacts(LN, LE, LW);
		AStructuralActor* North = Room[1].Get();
		AStructuralActor* East = Room[3].Get();
		AStructuralActor* West = Room[4].Get();
		UE_LOG(LogAfterfall, Log,
			TEXT("ROOMSERVER READY north=%d east=%d west=%d floor=%d south=%d (live, %d clients)"),
			North->DetachedCells.Num(), East->DetachedCells.Num(), West->DetachedCells.Num(),
			Room[0].Get()->DetachedCells.Num(), Room[2].Get()->DetachedCells.Num(), Open);
		// Heartbeat: proves the server (and its room state) stays up while
		// the connected clients observe it.
		W->GetTimerManager().SetTimer(ServerWaitTimer, this,
			&UDestructionRoomSubsystem::StepRoomServerHeartbeat, 10.0f, true);
		return;
	}
	if (ServerWaitPolls >= 360) // 180s: clients never arrived
	{
		W->GetTimerManager().ClearTimer(ServerWaitTimer);
		ServerWaitTimer.Invalidate();
		UE_LOG(LogAfterfall, Error,
			TEXT("ROOMSERVER FAIL wait timeout (%d/%d clients after 180s)"), Open, ServerWaitTarget);
		FPlatformMisc::RequestExitWithStatus(true, 1);
	}
}

void UDestructionRoomSubsystem::ClientCheck(bool bOk, const TCHAR* Name, const FString& Detail)
{
	UE_LOG(LogAfterfall, Log, TEXT("%s %s %s (%s)"), ClientTag,
		bOk ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
	if (bOk)
	{
		++ClientPass;
	}
	else
	{
		++ClientFail;
	}
}

AStructuralActor* UDestructionRoomSubsystem::FindClientPart(const TArray<AStructuralActor*>& Actors,
	const FVector& At, float YawDeg) const
{
	for (AStructuralActor* A : Actors)
	{
		if (!A)
		{
			continue;
		}
		if (!A->GetActorLocation().Equals(At, 2.f))
		{
			continue;
		}
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(A->GetActorRotation().Yaw, YawDeg)) > 1.f)
		{
			continue;
		}
		return A;
	}
	return nullptr;
}

void UDestructionRoomSubsystem::RunRoomClientVerify()
{
	StartClientVerify(TEXT("ROOMCLIENT"), false);
}

void UDestructionRoomSubsystem::RunRoomLiveClient()
{
	StartClientVerify(TEXT("ROOMLIVE"), true);
}

void UDestructionRoomSubsystem::StartClientVerify(const TCHAR* Tag, bool bLive)
{
	UWorld* W = GetWorld();
	// Pre-join worlds (standalone/transitional) are not verifiable yet: each
	// new world re-runs Initialize, so the post-travel client world retries.
	if (!W || !W->IsNetMode(NM_Client))
	{
		return;
	}
	if (ClientTimer.IsValid())
	{
		return; // already polling
	}
	ClientTag = Tag;
	bLiveMode = bLive;
	ClientPolls = 0;
	ClientPass = 0;
	ClientFail = 0;
	ClientNorthBefore = INDEX_NONE;
	ClientSettledPolls = 0;
	bClientBypassSent = false;
	bClientCoreChecked = false;
	bBaselineSeen = false;
	LiveWaitPolls = 0;
	UE_LOG(LogAfterfall, Log, TEXT("%s verify started (%s)"), ClientTag,
		bLiveMode ? TEXT("waiting for intact baseline, then live impact") : TEXT("waiting for replicated room"));
	W->GetTimerManager().SetTimer(ClientTimer, this,
		&UDestructionRoomSubsystem::StepClientVerify, 0.5f, true);
}

void UDestructionRoomSubsystem::StepClientVerify()
{
	UWorld* W = GetWorld();
	if (!W || !ClientTimer.IsValid())
	{
		return;
	}
	++ClientPolls;
	TArray<AStructuralActor*> Actors;
	for (TActorIterator<AStructuralActor> It(W); It; ++It)
	{
		Actors.Add(*It);
	}
	if (Actors.Num() < 5)
	{
		if (bClientCoreChecked || bBaselineSeen)
		{
			// Regression: actors were previously observed, now gone.
		const APlayerController* PC = W->GetFirstPlayerController();
		UE_LOG(LogAfterfall, Warning,
			TEXT("%s lost room actors (had state, now %d/5, netmode=%d, pc=%d, driver=%d)"),
				ClientTag, Actors.Num(), (int32)W->GetNetMode(),
				PC != nullptr, W->GetNetDriver() != nullptr);
		}
		if (ClientPolls % 20 == 0)
		{
			UE_LOG(LogAfterfall, Log, TEXT("%s waiting for room actors (%d/5, poll %d)"),
				ClientTag, Actors.Num(), ClientPolls);
		}
		if (ClientPolls >= 120)
		{
			ClientCheck(false, TEXT("room replicates"),
				FString::Printf(TEXT("only %d/5 actors after 60s"), Actors.Num()));
			FinishClientVerify();
		}
		return;
	}
	AStructuralActor* Floor = FindClientPart(Actors, FVector(RoomX, 0.f, 100.f), 0.f);
	AStructuralActor* North = FindClientPart(Actors, FVector(RoomX, 300.f, WallZ), 0.f);
	AStructuralActor* South = FindClientPart(Actors, FVector(RoomX, -300.f, WallZ), 0.f);
	AStructuralActor* East = FindClientPart(Actors, FVector(RoomX + 300.f, 0.f, WallZ), 0.f);
	AStructuralActor* West = FindClientPart(Actors, FVector(RoomX - 300.f, -150.f, WallZ), 90.f);
	if (!Floor || !North || !South || !East || !West)
	{
		if (ClientPolls >= 120)
		{
			ClientCheck(false, TEXT("room roles match"), TEXT("positions/rotations did not match after 60s"));
			FinishClientVerify();
		}
		return;
	}
	if (bLiveMode && !bBaselineSeen)
	{
		// Live proof, part 1: the client must first observe the room INTACT.
		// A first sight that is already damaged means the impact fired
		// before this client finished joining - fail loudly, never pass.
		const bool bAnyDamage = North->DetachedCells.Num() > 0
			|| East->DetachedCells.Num() > 0 || West->DetachedCells.Num() > 0;
		if (bAnyDamage)
		{
			ClientCheck(false, TEXT("live window intact"),
				TEXT("first sight already damaged - missed live window"));
			FinishClientVerify();
			return;
		}
		bBaselineSeen = true;
		LiveWaitPolls = 0;
		UE_LOG(LogAfterfall, Log, TEXT("%s baseline observed (room intact), waiting for impact"),
			ClientTag);
		return;
	}
	if (bLiveMode && North->DetachedCells.Num() == 0)
	{
		// Live proof, part 2: wait for the server-driven transition.
		++LiveWaitPolls;
		if (LiveWaitPolls >= 120)
		{
			ClientCheck(false, TEXT("live transition"),
				TEXT("impact never arrived after 60s"));
			FinishClientVerify();
		}
		return;
	}
	if (!bClientCoreChecked)
	{
		bClientCoreChecked = true;
		// Late-join proof: the client connected AFTER the impacts, so every
		// cell here arrived as replicated state, never as a live event.
		// Live proof, part 3: the transition above was observed live, so the
		// state below arrived as a live replication event, not initial state.
		UE_LOG(LogAfterfall, Log,
			TEXT("%s %s (north=%d east=%d west=%d floor=%d south=%d)"),
			ClientTag,
			bLiveMode ? TEXT("live transition observed") : TEXT("late-join state received"),
			North->DetachedCells.Num(), East->DetachedCells.Num(), West->DetachedCells.Num(),
			Floor->DetachedCells.Num(), South->DetachedCells.Num());
		ClientCheck(!North->HasAuthority(), TEXT("client has no authority"),
			FString::Printf(TEXT("netmode=%d"), (int32)W->GetNetMode()));
		ClientCheck(North->DetachedCells.Num() > 0, TEXT("replicated melee detaches"),
			FString::Printf(TEXT("north %d cells"), North->DetachedCells.Num()));
		ClientCheck(East->DetachedCells.Num() > 0, TEXT("replicated hitscan detaches"),
			FString::Printf(TEXT("east %d cells"), East->DetachedCells.Num()));
		bool bSame = North->DetachedCells.Num() == West->DetachedCells.Num();
		for (int32 i = 0; bSame && i < North->DetachedCells.Num(); ++i)
		{
			bSame = North->DetachedCells[i] == West->DetachedCells[i];
		}
		ClientCheck(bSame, TEXT("replicated rotated wall matches"),
			FString::Printf(TEXT("west %d, north %d cells"),
				West->DetachedCells.Num(), North->DetachedCells.Num()));
		ClientCheck(Floor->DetachedCells.Num() == 0 && South->DetachedCells.Num() == 0,
			TEXT("intact cells remain intact"),
			FString::Printf(TEXT("floor %d south %d"),
				Floor->DetachedCells.Num(), South->DetachedCells.Num()));
		bool bSync = true;
		for (AStructuralActor* A : { Floor, North, South, East, West })
		{
			bSync = bSync && A->Destruction
				&& A->Destruction->IsInSync(A->DetachedCells)
				&& A->Destruction->VisibleCellCount() == 27 - A->DetachedCells.Num();
		}
		ClientCheck(bSync, TEXT("replicated visuals in sync"), TEXT("5/5 IsInSync + instance counts"));
		const int32 Det = North->DetachedCells[0];
		FString HoleDetail;
		AActor* ThroughHole = TraceThroughCell(North, Det, FVector(0.f, 1.f, 0.f), HoleDetail);
		ClientCheck(ThroughHole == nullptr, TEXT("replicated hole has no collision"), HoleDetail);
		int32 Intact = INDEX_NONE;
		for (int32 i = 0; i < 27; ++i)
		{
			if (!North->DetachedCells.Contains(i))
			{
				Intact = i;
				break;
			}
		}
		FString SolidDetail;
		AActor* OnSolid = TraceThroughCell(North, Intact, FVector(0.f, 1.f, 0.f), SolidDetail);
		ClientCheck(Intact != INDEX_NONE && OnSolid == North,
			TEXT("replicated intact cell blocks"), SolidDetail);
		// Bypass attempts: direct local commit must fail the authority gate,
		// and an invalid cell set sent via the Server RPC must change nothing.
		FDestructionEvent Bogus;
		Bogus.Direction = FVector::ZeroVector;
		const int32 DirectRet = North->ServerApplyImpact(Bogus, 10.f, FGuid::NewGuid());
		ClientCheck(DirectRet == 0, TEXT("client impact rejected locally"),
			FString::Printf(TEXT("ret=%d"), DirectRet));
		ClientNorthBefore = North->DetachedCells.Num();
		North->ServerApplyDestruction({ 99, -5 }, 10.f, FGuid::NewGuid());
		bClientBypassSent = true;
		ClientSettledPolls = 0;
		return;
	}
	if (bClientBypassSent)
	{
		++ClientSettledPolls;
		if (ClientSettledPolls >= 4) // ~2s for the RPC round-trip + replication
		{
			ClientCheck(North->DetachedCells.Num() == ClientNorthBefore,
				TEXT("invalid client commit ignored"),
				FString::Printf(TEXT("north still %d cells"), North->DetachedCells.Num()));
			FinishClientVerify();
		}
		return;
	}
	if (ClientPolls >= 120)
	{
		ClientCheck(false, TEXT("client verify timeout"), TEXT("no result after 60s"));
		FinishClientVerify();
	}
}

void UDestructionRoomSubsystem::FinishClientVerify()
{
	UWorld* W = GetWorld();
	if (W)
	{
		W->GetTimerManager().ClearTimer(ClientTimer);
	}
	ClientTimer.Invalidate();
	const FString Word = ClientFail == 0 ? TEXT("PASS") : TEXT("FAIL");
	UE_LOG(LogAfterfall, Log, TEXT("%s SUMMARY pass=%d fail=%d %s %s"),
		ClientTag, ClientPass, ClientFail, ClientTag, *Word);
	if (ClientFail == 0)
	{
		if (UWorld* W2 = GetWorld())
		{
			if (APlayerController* PC = W2->GetFirstPlayerController())
			{
				PC->ConsoleCommand(TEXT("quit"), true);
			}
		}
		FPlatformMisc::RequestExit(0);
	}
	else
	{
		// Nonzero process exit on failure so runners can gate on ERRORLEVEL.
		FPlatformMisc::RequestExitWithStatus(true, 1);
	}
}

static void AFPostWorldInit(UWorld* W, const UWorld::InitializationValues IVS)
{
	if (!W)
	{
		return;
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("afRoomSelfTest")))
	{
		return;
	}
	if (UDestructionRoomSubsystem* S = W->GetSubsystem<UDestructionRoomSubsystem>())
	{
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, S, &UDestructionRoomSubsystem::RunRoomSelfTest, 1.0f, false);
	}
}

static struct FAFRoomHook
{
	FAFRoomHook()
	{
		FWorldDelegates::OnPostWorldInitialization.AddStatic(&AFPostWorldInit);
	}
} GAFRoomHook;

static FAutoConsoleCommandWithWorld RoomBuildCmd(
	TEXT("afBuildRoom"),
	TEXT("AFTERFALL dev: build the destructible graybox room fixture"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UDestructionRoomSubsystem* S = W->GetSubsystem<UDestructionRoomSubsystem>())
		{
			S->BuildRoom();
		}
	}));

static FAutoConsoleCommandWithWorld RoomServerCmd(
	TEXT("afRoomServer"),
	TEXT("AFTERFALL dev: room replication server (build + impact, stay up for client)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UDestructionRoomSubsystem* S = W->GetSubsystem<UDestructionRoomSubsystem>())
		{
			S->RunRoomServer();
		}
	}));

static FAutoConsoleCommandWithWorld RoomClientVerifyCmd(
	TEXT("afRoomClientVerify"),
	TEXT("AFTERFALL dev: verify replicated room state on a connected client"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UDestructionRoomSubsystem* S = W->GetSubsystem<UDestructionRoomSubsystem>())
		{
			S->RunRoomClientVerify();
		}
	}));

static FAutoConsoleCommandWithWorld RoomLiveClientCmd(
	TEXT("afRoomLiveClient"),
	TEXT("AFTERFALL dev: live-multiclient check (baseline intact, then transition)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UDestructionRoomSubsystem* S = W->GetSubsystem<UDestructionRoomSubsystem>())
		{
			S->RunRoomLiveClient();
		}
	}));

static FAutoConsoleCommandWithWorld RoomSelfTestCmd(
	TEXT("afRoomSelfTest"),
	TEXT("AFTERFALL dev: end-to-end room destruction self-test (melee/hitscan/collision/save/reload)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UDestructionRoomSubsystem* S = W->GetSubsystem<UDestructionRoomSubsystem>())
		{
			S->RunRoomSelfTest();
		}
	}));
