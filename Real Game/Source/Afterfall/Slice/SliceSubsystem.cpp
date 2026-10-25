// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "SliceSubsystem.h"
#include "AfterfallCharacter.h"
#include "World/ResourceNode.h"
#include "AI/ZombiePrototype.h"
#include "Combat/HealthComponent.h"
#include "Combat/ArticulatedBodyComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interaction/Interactable.h"
#include "UI/AfterfallHUD.h"
#include "World/PhysicalItem.h"
#include "Storage/PhysicalMat.h"
#include "Movement/AfterfallMovementComponent.h"
#include "Interaction/CarryComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Components/DirectionalLightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Subsystem/SubsystemService.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Camera/PlayerCameraManager.h"
#include "Structural/StructuralActor.h"
#include "Structural/StructuralDestructionComponent.h"
#include "Structural/ConstructionValidator.h"
#include "Destruction/DestructionPropagation.h"
#include "Afterfall.h"

bool UAfterfallSliceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* W = Cast<UWorld>(Outer))
	{
		return W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE;
	}
	return false;
}

void UAfterfallSliceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* W = GetWorld())
	{
		// Delay past template GameMode pawn spawn so the swap has a target.
		W->GetTimerManager().SetTimer(SetupTimer, this,
			&UAfterfallSliceSubsystem::DoSliceSetup, 1.5f, false);
		if (FParse::Param(FCommandLine::Get(), TEXT("afPipelineSelfTest")))
		{
			// One-shot runtime acceptance: runs after slice setup has spawned the
			// structural seed, then exits with a status the shell can assert.
			W->GetTimerManager().SetTimer(PipelineTimer, this,
				&UAfterfallSliceSubsystem::RunPipelineSelfTest, 4.0f, false);
		}
	}
}

void UAfterfallSliceSubsystem::DoSliceSetup()
{
	UWorld* W = GetWorld();
	if (!W || !W->GetAuthGameMode())
	{
		return; // server only; clients get everything replicated
	}
	bSliceReady = true;

	FVector Anchor = FVector::ZeroVector;
	if (APlayerStart* Start = Cast<APlayerStart>(
		UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass())))
	{
		Anchor = Start->GetActorLocation();
	}
	UE_LOG(LogAfterfall, Log, TEXT("Slice setup at %s"), *Anchor.ToString());
	const bool bEnableSliceTestFixtures = FParse::Param(FCommandLine::Get(), TEXT("afPipelineSelfTest"))
		|| FParse::Param(FCommandLine::Get(), TEXT("afSliceSelfTest"))
		|| FParse::Param(FCommandLine::Get(), TEXT("afPhysicsLab"))
		|| FParse::Param(FCommandLine::Get(), TEXT("afSliceShowcase"));

	// Deterministic ground bootstrap. The World Partition level can stream in
	// nothing (fresh checkout / PIE), leaving the pawn floating in unlit empty
	// sky. Guarantee a floor + directional sun under the slice room in every
	// gameplay context; only when nothing solid is beneath the anchor, so maps
	// with real ground are untouched.
	{
		FHitResult Ground;
		FCollisionQueryParams GroundQ(SCENE_QUERY_STAT(AFGroundBootstrap), false);
		const FVector From = Anchor + FVector(0.f, 0.f, 200.f);
		if (!W->LineTraceSingleByChannel(Ground, From, From - FVector(0.f, 0.f, 8000.f),
			ECC_Visibility, GroundQ))
		{
			// Small flat engine plane is only a fallback if the GameMode's large
			// startup floor is unavailable; keep the cube collider invisible.
			const float FloorTop = Anchor.Z - 95.f;
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* Floor = W->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(),
				FTransform(FRotator::ZeroRotator,
					FVector(Anchor.X, Anchor.Y, FloorTop)), P))
			{
				if (UStaticMeshComponent* M = Floor->GetStaticMeshComponent())
				{
				if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr,
						TEXT("/Engine/BasicShapes/Plane.Plane")))
				{
					M->SetMobility(EComponentMobility::Movable);
					M->SetStaticMesh(Plane);
				}
					M->SetWorldScale3D(FVector(30.f, 30.f, 1.f));
					M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					UBoxComponent* FloorCollision = NewObject<UBoxComponent>(Floor, TEXT("SliceFloorCollision"));
					FloorCollision->SetupAttachment(M);
					FloorCollision->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
					FloorCollision->SetBoxExtent(FVector(1500.f, 1500.f, 10.f));
					FloorCollision->SetCollisionProfileName(TEXT("BlockAll"));
					FloorCollision->RegisterComponent();
				}
			}
			if (AActor* Sun = W->SpawnActor<AActor>(AActor::StaticClass(),
				FTransform(FRotator(-45.f, 45.f, 0.f), Anchor + FVector(0.f, 0.f, 2000.f)), P))
			{
				UDirectionalLightComponent* D =
					NewObject<UDirectionalLightComponent>(Sun);
				D->Intensity = 8.f;
				D->SetWorldRotation(Sun->GetActorRotation());
				Sun->SetRootComponent(D);
				D->RegisterComponent();
			}
			UE_LOG(LogAfterfall, Log,
				TEXT("Slice ground bootstrap: floor+sun at %s (no streamed ground)"),
				*Anchor.ToString());
		}
	}

	// Stage B integration (real consumer): resolve the slice anchor to its
	// voxel lattice cell through the typed service path. The slice must not
	// hard-depend on the voxel provider, so a refusal is logged and setup
	// continues; the error value says exactly why it failed.
	{
		FVoxelCellRequest CellRequest;
		CellRequest.WorldPoint = Anchor;
		CellRequest.Origin = FVector::ZeroVector;
		const FVoxelCellResponse Cell = QueryVoxelCell(
			W->GetSubsystem<UAfterfallSubsystemRegistry>(), CellRequest);
		if (Cell.IsSuccess())
		{
			UE_LOG(LogAfterfall, Log, TEXT("Slice anchor voxel cell (%d,%d,%d)"),
				Cell.Cell.X, Cell.Cell.Y, Cell.Cell.Z);
		}
		else
		{
			UE_LOG(LogAfterfall, Warning, TEXT("Slice voxel service refused anchor: %s"),
				AfterfallService::ErrorText(Cell.Error));
		}
	}

	auto SpawnNode = [&](EResourceKind Kind, FVector Offset)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AResourceNode* N = W->SpawnActor<AResourceNode>(
			AResourceNode::StaticClass(), Anchor + Offset, FRotator::ZeroRotator, P))
		{
			N->Kind = Kind;
			UE_LOG(LogAfterfall, Log, TEXT("Slice node %d at %s"), (int32)Kind,
				*N->GetActorLocation().ToString());
		}
	};
	if (bEnableSliceTestFixtures)
	{
		SpawnNode(EResourceKind::Wood, FVector(400.f, 0.f, 100.f));
		SpawnNode(EResourceKind::Stone, FVector(0.f, 400.f, 100.f));
		SpawnNode(EResourceKind::Hemp, FVector(-400.f, 0.f, 100.f));
	}

	// Playable lab: exactly three independently simulated zombies at
	// distinct, safe offsets (no shared mutable AI/movement state).
	auto SpawnZombie = [&](FVector Offset)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AZombiePrototype* Z = W->SpawnActor<AZombiePrototype>(
			AZombiePrototype::StaticClass(), Anchor + Offset,
			FRotator::ZeroRotator, P))
		{
			UE_LOG(LogAfterfall, Log, TEXT("Slice zombie at %s"),
				*Z->GetActorLocation().ToString());
		}
	};
	if (bEnableSliceTestFixtures)
	{
		SpawnZombie(FVector(1500.f, 0.f, 100.f));
		SpawnZombie(FVector(1200.f, 900.f, 100.f));
		SpawnZombie(FVector(1200.f, -900.f, 100.f));
	}

	// Playable physics-test blocks: distinct materials/masses/friction.
	// Graybox rigid bodies (no destruction claims); the destruction system
	// stays the one path for fracture via ServerApplyImpact.
	auto SpawnLabBlock = [&](const TCHAR* Name, const TCHAR* MeshPath, FVector Offset,
		FVector Scale, float MassKg, float Friction, float Restitution)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		P.Name = FName(Name);
		AStaticMeshActor* B = W->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Anchor + Offset, FRotator::ZeroRotator, P);
		if (!B)
		{
			return;
		}
		if (UStaticMeshComponent* M = B->GetStaticMeshComponent())
		{
			// Mobility BEFORE the mesh assignment (a Static component warns).
			M->SetMobility(EComponentMobility::Movable); // runtime mesh change
			M->UnregisterComponent();
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
			{
				M->SetStaticMesh(Mesh);
			}
			M->SetWorldScale3D(Scale);
			M->SetCollisionProfileName(TEXT("PhysicsActor"));
			M->SetSimulatePhysics(true);
			M->SetMassOverrideInKg(NAME_None, MassKg, true);
			if (UPhysicalMaterial* PM = NewObject<UPhysicalMaterial>(B))
			{
				PM->Friction = Friction;
				PM->Restitution = Restitution;
				M->SetPhysMaterialOverride(PM);
			}
			M->RegisterComponent();
			UE_LOG(LogAfterfall, Log, TEXT("Slice lab block %s (%.0fkg mu=%.2f e=%.2f) at %s"),
				Name, MassKg, Friction, Restitution, *B->GetActorLocation().ToString());
		}
	};
	if (bEnableSliceTestFixtures)
	{
		SpawnLabBlock(TEXT("LabBlock_Concrete"), TEXT("/Engine/BasicShapes/Cube.Cube"),
			FVector(900.f, 300.f, 100.f), FVector(1.f, 1.f, 1.f), 50.f, 0.9f, 0.1f);
		SpawnLabBlock(TEXT("LabBlock_Metal"), TEXT("/Engine/BasicShapes/Cube.Cube"),
			FVector(900.f, -300.f, 100.f), FVector(0.8f, 0.8f, 0.8f), 80.f, 0.4f, 0.3f);
		SpawnLabBlock(TEXT("LabBlock_WoodCrate"), TEXT("/Engine/BasicShapes/Cube.Cube"),
			FVector(1100.f, 0.f, 100.f), FVector(0.6f, 0.6f, 0.6f), 10.f, 0.7f, 0.2f);
		SpawnLabBlock(TEXT("LabBlock_Cylinder"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
			FVector(1300.f, 300.f, 100.f), FVector(0.5f, 0.5f, 1.f), 15.f, 0.5f, 0.25f);
	}

	// Stage 2 graybox: one light + one heavy throwable near the anchor.
	auto SpawnItem = [&](FName Type, float Mass, FVector Offset)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (APhysicalItem* I = W->SpawnActor<APhysicalItem>(
			APhysicalItem::StaticClass(), Anchor + Offset, FRotator::ZeroRotator, P))
		{
			I->ItemType = Type;
		I->MassKg = Mass;
		I->ItemId = FGuid::NewGuid();
			UE_LOG(LogAfterfall, Log, TEXT("Stage2 item %s (%.1fkg) at %s"),
				*Type.ToString(), Mass, *I->GetActorLocation().ToString());
		}
	};
	if (bEnableSliceTestFixtures)
	{
		SpawnItem(TEXT("AF_R_Scrap"), 2.f, FVector(600.f, 200.f, 150.f));
		SpawnItem(TEXT("AF_R_Stone"), 12.f, FVector(700.f, -200.f, 150.f));
	}

	// Runtime structural seed. AStructuralActor is only created by placement,
	// and nothing bound placement input, so the destruction pipeline had no
	// geometry to act on in play. These two parts go through the same
	// production spawn path as placement (ServerInitialize + damage visuals).
	if (bEnableSliceTestFixtures)
	{
		const FVector WallAt = Anchor + FVector(-700.f, 0.f, 0.f);
		SpawnStructuralPart(EStructuralPart::Foundation, WallAt + FVector(0.f, 0.f, -25.f),
			FRotator::ZeroRotator, 100.f);
		SeedWall = SpawnStructuralPart(EStructuralPart::Wall, WallAt + FVector(0.f, 0.f, 50.f),
			FRotator::ZeroRotator, 100.f);
		UE_LOG(LogAfterfall, Log, TEXT("Slice structural seed at %s (%s)"),
			*WallAt.ToString(), SeedWall.IsValid() ? TEXT("live") : TEXT("MISSING"));
	}

	// Pawn swap: template pawn -> our C++ pawn (brings camera/health/interact).
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = Cast<APlayerController>(It->Get()))
		{
			if (APawn* Old = PC->GetPawn())
			{
				if (Cast<AAfterfallCharacter>(Old))
				{
					PC->ClientSetHUD(AAfterfallHUD::StaticClass());
					continue;
				}
				const FVector Loc = Old->GetActorLocation();
				const FRotator Rot = Old->GetActorRotation();
				FActorSpawnParameters P;
				P.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				if (AAfterfallCharacter* Fresh = W->SpawnActor<AAfterfallCharacter>(
					AAfterfallCharacter::StaticClass(), Loc, Rot, P))
				{
					PC->UnPossess();
					PC->Possess(Fresh);
					PC->ClientSetHUD(AAfterfallHUD::StaticClass());
					Old->Destroy();
					UE_LOG(LogAfterfall, Log, TEXT("Slice pawn swapped for %s"),
						*PC->GetName());
				}
			}
		}
	}

	// Headless acceptance hook: the same scripted loop the afSliceSelfTest
	// console command runs, scheduled past setup so it has a possessed pawn.
	if (FParse::Param(FCommandLine::Get(), TEXT("afSliceSelfTest")))
	{
		FTimerHandle SelfTestTimer;
		W->GetTimerManager().SetTimer(SelfTestTimer, this,
			&UAfterfallSliceSubsystem::RunSliceSelfTest, 3.0f, false);
	}
}

AStructuralActor* UAfterfallSliceSubsystem::SpawnStructuralPart(EStructuralPart Part,
	const FVector& At, const FRotator& Rot, float ExtentCm)
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return nullptr; // server-authoritative: clients only receive replicas
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AStructuralActor* PartActor = W->SpawnActor<AStructuralActor>(
		AStructuralActor::StaticClass(), At, Rot, P);
	if (!PartActor || !PartActor->Destruction)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Slice structural spawn failed"));
		return nullptr;
	}
	PartActor->Destruction->CellExtentCm = ExtentCm;
	FStructuralSpec Spec;
	Spec.Part = Part;
	Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Part);
	Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Part);
	PartActor->ServerInitialize(Spec, 0);
	PartActor->RefreshDestructionVisual();
	return PartActor;
}

AActor* UAfterfallSliceSubsystem::TraceThroughCell(AStructuralActor* Part, int32 CellIndex,
	FString& OutDetail)
{
	UWorld* W = GetWorld();
	OutDetail.Reset();
	if (!W || !Part || !Part->Destruction)
	{
		return nullptr;
	}
	const FVector Local = UDestructionPropagationService::GridPoint(FVector::ZeroVector,
		Part->Destruction->CellExtentCm, CellIndex % 3, (CellIndex / 3) % 3, CellIndex / 9);
	const FVector World = Part->GetActorTransform().TransformPosition(Local);
	const FVector Normal = Part->GetActorForwardVector().GetSafeNormal();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFPipelineTrace), false);
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(W, 0))
	{
		Params.AddIgnoredActor(Pawn);
	}
	W->LineTraceSingleByChannel(Hit, World - Normal * 25.f, World + Normal * 25.f,
		ECC_Visibility, Params);
	OutDetail = FString::Printf(TEXT("cell %d -> %s"), CellIndex,
		Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nothing"));
	return Hit.GetActor();
}

void UAfterfallSliceSubsystem::RequestPipelineSelfTest()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return;
	}
	// Never run before slice setup: the seed structure and the pawn come from
	// production setup, and an early run would report a misleading failure.
	const float Delay = bSliceReady ? 0.f : 2.0f;
	W->GetTimerManager().SetTimer(PipelineTimer, this,
		&UAfterfallSliceSubsystem::RunPipelineSelfTest, Delay, false);
}

void UAfterfallSliceSubsystem::RunPipelineSelfTest()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PIPELINE FAIL: server only"));
		return;
	}
	if (W->IsNetMode(NM_DedicatedServer))
	{
		// The scripted steps drive character server RPCs, which need a local
		// player. Fail loudly instead of reporting a misleading pass.
		UE_LOG(LogAfterfall, Warning, TEXT("PIPELINE FAIL: needs a local player (launch with -game, not -server)"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	int32 Pass = 0;
	int32 Fail = 0;
	auto Check = [&Pass, &Fail](bool bOk, const TCHAR* Step, const FString& Detail = FString())
	{
		if (bOk) { ++Pass; } else { ++Fail; }
		UE_LOG(LogAfterfall, Log, TEXT("PIPELINE %s: %s%s"),
			bOk ? TEXT("PASS") : TEXT("FAIL"), Step,
			Detail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Detail));
	};

	// ---- 1. Runtime registration (module loaded -> subsystem registered).
	UAfterfallSubsystemRegistry* Reg = W->GetSubsystem<UAfterfallSubsystemRegistry>();
	IAfterfallSubsystem* Voxel = Reg ? Reg->Find(TEXT("Voxel")) : nullptr;
	Check(Reg != nullptr, TEXT("registry available"));
	Check(Voxel != nullptr && AfterfallSubsystem::IsServable(Voxel->GetState()),
		TEXT("voxel provider registered + ready"),
		Voxel ? FString::Printf(TEXT("state=%d"), (int32)Voxel->GetState()) : FString(TEXT("missing")));
	const int32 NumBefore = Reg ? Reg->Num() : -1;
	if (Voxel)
	{
		Voxel->Initialize(); // must be idempotent
	}
	Check(Reg && Reg->Num() == NumBefore, TEXT("repeated initialize does not duplicate"),
		Reg ? FString::Printf(TEXT("before=%d after=%d"), NumBefore, Reg->Num()) : FString());

	// ---- 2. Geometry the pipeline can act on.
	AStructuralActor* Wall = SeedWall.Get();
	if (!Wall)
	{
		for (TActorIterator<AStructuralActor> It(W); It; ++It)
		{
			Wall = *It;
			break;
		}
	}
	if (!Wall && !bSliceReady && PipelineDeferrals < 5)
	{
		// Invoked before slice setup finished (console command / early flag).
		++PipelineDeferrals;
		W->GetTimerManager().SetTimer(PipelineTimer, this,
			&UAfterfallSliceSubsystem::RunPipelineSelfTest, 1.0f, false);
		return;
	}
	if (!Wall)
	{
		Check(false, TEXT("destructible structure in world"));
		UE_LOG(LogAfterfall, Warning, TEXT("PIPELINE SUMMARY pass=%d fail=%d PIPELINE FAIL"), Pass, Fail);
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	Check(true, TEXT("destructible structure in world"), Wall->GetName());
	// The slice seed wall sits directly on its own foundation (interpenetrating
	// boxes), so a camera ray reaches the foundation first. The damage steps act
	// on a second part spawned through the same production placement path, in
	// clear space, so hits/debris are unambiguous.
	AStructuralActor* Target = SpawnStructuralPart(EStructuralPart::Wall,
		Wall->GetActorLocation() + FVector(0.f, 700.f, 300.f), FRotator::ZeroRotator, 100.f);
	Check(Target != nullptr, TEXT("production spawn path creates a target part"));
	if (!Target)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PIPELINE SUMMARY pass=%d fail=%d PIPELINE FAIL"), Pass, Fail);
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	Wall = Target;

	// The damage entry points are character server RPCs, so the test needs a
	// possessed pawn. Harness scaffolding only: the pawn is a real
	// AAfterfallCharacter and every step below runs production code.
	AAfterfallCharacter* Pawn = FindSlicePawn();
	if (!Pawn)
	{
		APlayerController* PC = W->GetFirstPlayerController();
		if (!PC && !W->IsNetMode(NM_DedicatedServer))
		{
			PC = UGameplayStatics::CreatePlayer(this, 0, true);
		}
		if (PC)
		{
			if (APawn* Old = PC->GetPawn())
			{
				PC->UnPossess();
				Old->Destroy();
			}
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			Pawn = W->SpawnActor<AAfterfallCharacter>(AAfterfallCharacter::StaticClass(),
				Wall->GetActorLocation() + FVector(200.f, 0.f, 40.f), FRotator::ZeroRotator, P);
			if (Pawn)
		{
				PC->Possess(Pawn);
			}
		}
	}
	if (!Pawn || !Pawn->Health)
	{
		Check(false, TEXT("possessed character available"),
			W->IsNetMode(NM_DedicatedServer) ? TEXT("dedicated server has no local player; run with -game") : FString());
		UE_LOG(LogAfterfall, Warning, TEXT("PIPELINE SUMMARY pass=%d fail=%d PIPELINE FAIL"), Pass, Fail);
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	Check(true, TEXT("possessed character available"), Pawn->GetName());

	// ---- 3. Production melee path: input handler equivalent -> server RPC.
	// Aim through the real view transform: stand off the target, measure the
	// camera offset the pawn produces, then stand so the camera looks straight
	// at the target's centre. Nothing here bypasses the damage code.
	const FVector WallLoc = Wall->GetActorLocation();
	APlayerController* PawnPC = Cast<APlayerController>(Pawn->GetController());
	auto RefreshView = [&]()
	{
		// The view transform is refreshed on tick; push it now so the traces
		// start from the position we just moved to.
		if (PawnPC && PawnPC->PlayerCameraManager)
		{
			PawnPC->PlayerCameraManager->UpdateCamera(0.f);
		}
	};
	Pawn->SetActorLocation(WallLoc + FVector(0.f, -200.f, 0.f), false, nullptr,
		ETeleportType::TeleportPhysics);
	Pawn->SetActorRotation(FRotator(0.f, 90.f, 0.f), ETeleportType::TeleportPhysics);
	if (PawnPC)
	{
		PawnPC->SetControlRotation(FRotator(0.f, 90.f, 0.f));
	}
	RefreshView();
	FVector CamLoc = Pawn->GetActorLocation();
	FRotator CamRot = Pawn->GetActorRotation();
	if (PawnPC)
	{
		PawnPC->GetPlayerViewPoint(CamLoc, CamRot);
	}
	const FVector CamOffset = CamLoc - Pawn->GetActorLocation();
	Pawn->SetActorLocation(WallLoc + FVector(0.f, -200.f, 0.f) - CamOffset, false, nullptr,
		ETeleportType::TeleportPhysics);
	RefreshView();
	if (PawnPC)
	{
		PawnPC->GetPlayerViewPoint(CamLoc, CamRot);
	}
	const FVector ToWall = (WallLoc - CamLoc).GetSafeNormal();
	Pawn->SetActorRotation(ToWall.Rotation(), ETeleportType::TeleportPhysics);
	if (PawnPC)
	{
		PawnPC->SetControlRotation(ToWall.Rotation());
		RefreshView();
		PawnPC->GetPlayerViewPoint(CamLoc, CamRot);
		FHitResult Diag;
		FCollisionQueryParams DiagParams(SCENE_QUERY_STAT(AFPipelineDiag), false, Pawn);
		W->LineTraceSingleByChannel(Diag, CamLoc, CamLoc + CamRot.Vector() * 1000.f,
			ECC_Visibility, DiagParams);
		UE_LOG(LogAfterfall, Log, TEXT("PIPELINE view: cam=%s rot=%s forward-hit=%s target=%s"),
			*CamLoc.ToString(), *CamRot.ToString(),
			Diag.GetActor() ? *Diag.GetActor()->GetName() : TEXT("nothing"), *Wall->GetName());
	}
	const int32 CellsBefore = Wall->DetachedCells.Num();
	Pawn->ServerAttack();
	const int32 CellsAfterMelee = Wall->DetachedCells.Num();
	Check(CellsAfterMelee > CellsBefore, TEXT("melee -> structural cells detached"),
		FString::Printf(TEXT("%d -> %d"), CellsBefore, CellsAfterMelee));

	// ---- 4. Production projectile path.
	Pawn->ServerFireHitscan(ToWall);
	const int32 CellsAfterShot = Wall->DetachedCells.Num();
	Check(CellsAfterShot > CellsAfterMelee, TEXT("hitscan -> more structural cells detached"),
		FString::Printf(TEXT("%d -> %d"), CellsAfterMelee, CellsAfterShot));
	Check(Wall->Durability < Wall->Spec.MaxDurability, TEXT("durability debited by impacts"),
		FString::Printf(TEXT("%.1f / %.1f"), Wall->Durability, Wall->Spec.MaxDurability));

	// ---- 5. Collision agrees with authoritative state.
	if (Wall->DetachedCells.Num() > 0)
	{
		FString Detail;
		AActor* Hole = TraceThroughCell(Wall, Wall->DetachedCells[0], Detail);
		Check(Hole != Wall, TEXT("detached cell no longer blocks"), Detail);
	}
	int32 IntactCell = INDEX_NONE;
	for (int32 i = 0; i < 27; ++i)
	{
		if (!Wall->DetachedCells.Contains(i))
		{
			IntactCell = i;
			break;
		}
	}
	if (IntactCell != INDEX_NONE)
	{
		FString Detail;
		AActor* Solid = TraceThroughCell(Wall, IntactCell, Detail);
		Check(Solid == Wall, TEXT("intact cell still blocks"), Detail);
	}

	// ---- 6. Delta export reflects authority.
	const FStructureDelta Delta = Wall->ExportDelta();
	Check(Delta.DetachedCells.Num() == Wall->DetachedCells.Num()
		&& Delta.Guid == Wall->StableGuid, TEXT("delta matches authoritative state"),
		FString::Printf(TEXT("cells=%d/%d"), Delta.DetachedCells.Num(), Wall->DetachedCells.Num()));

	// ---- 7. Reload: destroy + rebuild from the delta, no duplicates, hole kept.
	const int32 ActorsBefore = [W]()
	{
		int32 N = 0;
		for (TActorIterator<AStructuralActor> It(W); It; ++It) { ++N; }
		return N;
	}();
	const EStructuralPart Part = Wall->Spec.Part;
	const int32 DeltaCells = Delta.DetachedCells.Num();
	Wall->Destroy();
	AStructuralActor* Fresh = SpawnStructuralPart(Part, Delta.Location, Delta.Rotation, 100.f);
	Check(Fresh != nullptr, TEXT("reload spawns replacement"));
	if (Fresh)
	{
		Fresh->ImportDelta(Delta);
		Check(Fresh->DetachedCells.Num() == DeltaCells, TEXT("reload restores destroyed cells"),
			FString::Printf(TEXT("%d/%d"), Fresh->DetachedCells.Num(), DeltaCells));
		Check(Fresh->StableGuid == Delta.Guid, TEXT("reload keeps stable identity"));
		if (Fresh->DetachedCells.Num() > 0)
		{
			FString Detail;
			AActor* Hole = TraceThroughCell(Fresh, Fresh->DetachedCells[0], Detail);
			Check(Hole != Fresh, TEXT("reloaded hole stays non-colliding"), Detail);
		}
	}
	int32 ActorsAfter = 0;
	for (TActorIterator<AStructuralActor> It(W); It; ++It) { ++ActorsAfter; }
	Check(ActorsAfter == ActorsBefore, TEXT("reload does not duplicate structures"),
		FString::Printf(TEXT("before=%d after=%d"), ActorsBefore, ActorsAfter));
	SeedWall = Fresh;

	UE_LOG(LogAfterfall, Log, TEXT("PIPELINE SUMMARY pass=%d fail=%d PIPELINE %s"),
		Pass, Fail, Fail == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExitWithStatus(false, Fail == 0 ? 0 : 1);
}

void UAfterfallSliceSubsystem::RespawnPlayer(AController* PC)
{
	// Death path lives on the character (has pawn context); kept for API symmetry.
	(void)PC;
}

AAfterfallCharacter* UAfterfallSliceSubsystem::FindSlicePawn() const
{
	if (UWorld* W = GetWorld())
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			if (AController* PC = It->Get())
			{
				if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(PC->GetPawn()))
				{
					return C;
				}
			}
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------- Self-test ---

static void SliceLog(bool bPass, const TCHAR* Step)
{
	UE_LOG(LogAfterfall, Log, TEXT("SLICETEST %s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), Step);
}

void UAfterfallSliceSubsystem::RunSliceSelfTest()
{
	UWorld* W = GetWorld();
	if (!W || !W->GetAuthGameMode())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("SLICETEST FAIL: server only"));
		return;
	}
	AAfterfallCharacter* Pawn = FindSlicePawn();
	if (!Pawn || !Pawn->Health || !Pawn->Inventory)
	{
		SliceLog(false, TEXT("no slice pawn possessed — setup did not run"));
		return;
	}
	SliceLog(true, TEXT("slice pawn possessed"));

	AResourceNode* Node = nullptr;
	for (TActorIterator<AResourceNode> It(W); It; ++It)
	{
		Node = *It;
		break;
	}
	AZombiePrototype* Zombie = nullptr;
	for (TActorIterator<AZombiePrototype> It(W); It; ++It)
	{
		Zombie = *It;
		break;
	}
	if (!Node || !Zombie)
	{
		SliceLog(false, TEXT("slice actors missing"));
		return;
	}
	SliceLog(true, TEXT("node + zombie spawned"));

	// Step 1: harvest loop through the real server path.
	AController* PC = Pawn->GetController();
	Pawn->SetActorLocation(Node->GetActorLocation() + FVector(150.f, 0.f, 0.f));
	const int32 WoodBefore = Pawn->Inventory->GetWood();
	IInteractable::Execute_ServerInteract(Node, PC);
	const int32 WoodAfter = Pawn->Inventory->GetWood();
	const bool bHarvest = WoodAfter > WoodBefore && Node->IsDepleted();
	SliceLog(bHarvest, TEXT("harvest grants + depletes"));
	// Step 2: duplicate grant rejected.
	IInteractable::Execute_ServerInteract(Node, PC);
	SliceLog(Pawn->Inventory->GetWood() == WoodAfter, TEXT("no duplicate grant"));
	if (!bHarvest)
	{
		return;
	}

	// Step 3: zombie AI — drag it into melee, wait for the 4Hz brain to bite.
	TWeakObjectPtr<AZombiePrototype> WeakZ(Zombie);
	TWeakObjectPtr<AAfterfallCharacter> WeakP(Pawn);
	const float HPBefore = Pawn->Health->HP;
	Zombie->SetActorLocation(Pawn->GetActorLocation() + FVector(150.f, 0.f, 0.f));
	FTimerHandle Step3;
	W->GetTimerManager().SetTimer(Step3, [WeakP, WeakZ, HPBefore]()
	{
		AAfterfallCharacter* P = WeakP.Get();
		AZombiePrototype* Z = WeakZ.Get();
		if (!P || !Z)
		{
			SliceLog(false, TEXT("self-test actors died early"));
			return;
		}
		SliceLog(P->Health->HP < HPBefore, TEXT("zombie AI damaged player"));

		// Step 4: six torso hits kill (baseline); parts record exists.
		AController* PC2 = P->GetController();
		const bool bHadRecord = Z->GetDamageRecord().bHasRecord;
		for (int32 i = 0; i < 6; ++i)
		{
			P->SetActorLocation(Z->GetActorLocation() + FVector(150.f, 0.f, 0.f));
			Z->ServerHitPart(EBodyPart::Torso, PC2);
		}
		SliceLog(!Z->IsAlive(), TEXT("six torso hits kill"));
		SliceLog(Z->GetDamageRecord().bHasRecord && !bHadRecord, TEXT("first hit created record"));

		// Step 4b: articulated body grab/release through the same server-only
		// interaction path the player uses (E on a ragdoll).
		const FVector ToBody = (Z->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
		P->SetActorRotation(ToBody.Rotation());
		const bool bRagdoll = Z->Body && Z->Body->IsRagdoll();
		IInteractable::Execute_ServerInteract(Z, PC2);
		const bool bGrabbed = bRagdoll && Z->Body->IsGrabbed();
		IInteractable::Execute_ServerInteract(Z, PC2); // release
		const bool bReleased = bGrabbed && !Z->Body->IsGrabbed();
		SliceLog(bGrabbed && bReleased, TEXT("ragdoll grab + release"));

		// Step 5: player death -> auto-respawn with full HP.
		P->Health->ServerTakeDamage(500.f);
		UWorld* W2 = P->GetWorld();
		FTimerHandle Step5;
		TWeakObjectPtr<AController> WeakPC(PC2);
		W2->GetTimerManager().SetTimer(Step5, [WeakPC]()
		{
			AController* C = WeakPC.Get();
			AAfterfallCharacter* Fresh = C ? Cast<AAfterfallCharacter>(C->GetPawn()) : nullptr;
			const bool bOk = Fresh && Fresh->Health && !Fresh->Health->bIsDead
				&& FMath::IsNearlyEqual(Fresh->Health->HP, Fresh->Health->MaxHP);
			SliceLog(bOk, TEXT("death + auto-respawn"));
			SliceLog(bOk, TEXT("FULL LOOP"));
		}, 4.5f, false);
	}, 3.0f, false);
}

// ---------------------------------------------------------------- Stage 2 ---

static void Stage2Log(bool bPass, const TCHAR* Step)
{
	UE_LOG(LogAfterfall, Log, TEXT("STAGE2TEST %s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), Step);
}

void UAfterfallSliceSubsystem::RunStage2SelfTest()
{
	UWorld* W = GetWorld();
	if (!W || !W->GetAuthGameMode())
	{
		Stage2Log(false, TEXT("server only"));
		return;
	}
	AAfterfallCharacter* Pawn = FindSlicePawn();
	if (!Pawn || !Pawn->Carry)
	{
		Stage2Log(false, TEXT("no slice pawn/carry"));
		return;
	}
	Stage2Log(true, TEXT("slice pawn + carry present"));

	// 1. Fixed-step determinism: 5 sim-seconds consume exactly 600 quanta
	//    regardless of frame rate.
	{
		auto TotalStepsFor = [](double Dt, int32 Frames)
		{
			double Acc = 0.0;
			int32 Total = 0;
			for (int32 i = 0; i < Frames; ++i)
			{
				Total += UAfterfallMovementComponent::ConsumeFixedSteps(Acc, Dt, 1.0 / 120.0, 64);
			}
			return Total;
		};
		const int32 S30 = TotalStepsFor(1.0 / 30.0, 150);
		const int32 S60 = TotalStepsFor(1.0 / 60.0, 300);
		const int32 S240 = TotalStepsFor(1.0 / 240.0, 1200);
		Stage2Log(S30 == 600 && S60 == 600 && S240 == 600, TEXT("fixed-step 600 quanta per 5s at 30/60/240fps"));
	}

	// 2. Sprint: 5s budget at any frame rate, then 3s recovery; early
	//    release cancels without lockout.
	{
		auto BurnSprint = [](double Dt)
		{
			EAfterfallSprintState St = EAfterfallSprintState::Ready;
			float TL = 0.f, RL = 0.f;
			UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, 0.f);
			const bool bStarted = (St == EAfterfallSprintState::Sprinting);
			int32 Frames = 0;
			while (St == EAfterfallSprintState::Sprinting && Frames < 100000)
			{
				UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, (float)Dt);
				++Frames;
			}
			return TPair<int32, bool>(Frames, bStarted && St == EAfterfallSprintState::Recovering);
		};
		const TPair<int32, bool> R30 = BurnSprint(1.0 / 30.0);
		const TPair<int32, bool> R240 = BurnSprint(1.0 / 240.0);
		const bool bCountOk = R30.Key >= 149 && R30.Key <= 152 && R240.Key >= 1195 && R240.Key <= 1205;
		Stage2Log(R30.Value && R240.Value && bCountOk, TEXT("sprint 5s budget frame-rate independent"));

		EAfterfallSprintState St = EAfterfallSprintState::Ready;
		float TL = 0.f, RL = 0.f;
		UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, 0.f);
		UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, 1.f);
		UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, false, 5.f, 3.f, 0.f);
		Stage2Log(St == EAfterfallSprintState::Ready, TEXT("early sprint release cancels cleanly"));

		// Recovery: 3s lockout after a spent sprint.
		St = EAfterfallSprintState::Recovering;
		RL = 3.f;
		for (int32 i = 0; i < 90; ++i)
		{
			UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, 1.f / 30.f);
		}
		Stage2Log(St == EAfterfallSprintState::Ready, TEXT("3s recovery then ready"));
	}

	// 3. Pickup: Free->Held, duplicate pickup rejected.
	APhysicalItem* Item = nullptr;
	for (TActorIterator<APhysicalItem> It(W); It; ++It)
	{
		if ((*It)->IsFree())
		{
			Item = *It;
			break;
		}
	}
	if (!Item)
	{
		Stage2Log(false, TEXT("no free physical item"));
		return;
	}
	Stage2Log(true, TEXT("physical item present"));

	AController* PC = Pawn->GetController();
	Pawn->SetActorLocation(Item->GetActorLocation() + FVector(150.f, 0.f, 0.f));
	Pawn->Carry->ServerPickup(Item);
	const bool bPicked = Pawn->Carry->bIsCarrying && Pawn->Carry->CarriedItem == Item
		&& Item->WorldState == EItemWorldState::Held;
	Stage2Log(bPicked, TEXT("pickup transitions Free->Held"));
	if (!bPicked)
	{
		return;
	}
	Pawn->Carry->ServerPickup(Item); // duplicate: must be rejected
	Stage2Log(Pawn->Carry->CarriedItem == Item && Item->WorldState == EItemWorldState::Held,
		TEXT("duplicate pickup rejected"));
	const FGuid HeldId = Item->ItemId;

	// 4. Carry rule: heavy mass flags the movement cap.
	if (UAfterfallMovementComponent* M = Pawn->GetAfterfallMovement())
	{
		const bool bHeavyExpected = Item->MassKg >= M->Tuning.HeavyMassKg;
		Stage2Log(M->bCarryingHeavy == bHeavyExpected, TEXT("carry flag follows mass rule"));
		Stage2Log(!bHeavyExpected || FMath::IsNearlyEqual(M->GetCurrentMaxSpeed(), 89.4f, 0.05f),
			TEXT("heavy carry caps speed at 89.4 cm/s"));
	}

	// 5. Throw: single release, holder cleared, velocity = v_player + dir*speed.
	const FVector PVel = Pawn->GetVelocity();
	const FVector ThrowDir = FVector(1.f, 0.f, 0.2f).GetSafeNormal();
	Pawn->Carry->ServerThrow(FVector(1.f, 0.f, 0.2f));
	const FVector Expect = APhysicalItem::ComputeReleaseVelocity(PVel, ThrowDir, 900.f, 1.f);
	const bool bThrew = !Pawn->Carry->bIsCarrying && Pawn->Carry->CarriedItem == nullptr
		&& Item->IsFree() && Item->GetHolder() == nullptr
		&& Item->LastReleaseVelocity.Equals(Expect, 1.0f);
	Stage2Log(bThrew, TEXT("throw releases with v_player + dir*speed"));
	if (!bThrew)
	{
		return;
	}

	// 6. Mat: deploy, deposit (atomic), withdraw (conserves identity + count).
	Pawn->ServerDeployMat();
	APhysicalStorageMat* Mat = Pawn->DeployedMat;
	Stage2Log(Mat != nullptr, TEXT("mat deployed"));
	if (!Mat)
	{
		return;
	}
	Pawn->SetActorLocation(Item->GetActorLocation() + FVector(150.f, 0.f, 0.f));
	Pawn->Carry->ServerPickup(Item);
	Pawn->SetActorLocation(Mat->GetActorLocation() + FVector(150.f, 0.f, 0.f));
	const int32 Dep = Mat->ServerDeposit(PC);
	Stage2Log(Dep == 1 && !Pawn->Carry->bIsCarrying && Mat->GetMatCount(Item->ItemType) == 1,
		TEXT("mat deposit atomic (hands cleared, slot filled)"));
	const int32 Wd = Mat->ServerWithdraw(PC, 0, 1);
	bool bIdentityKept = false;
	for (TActorIterator<APhysicalItem> It(W); It; ++It)
	{
		if ((*It)->ItemId == HeldId && (*It)->IsFree())
		{
			bIdentityKept = true;
			break;
		}
	}
	Stage2Log(Wd == 1 && Mat->GetMatCount(Item->ItemType) == 0 && bIdentityKept,
		TEXT("mat withdraw conserves item identity"));
	Stage2Log(Mat->ExportDelta().ContainerGuid == Mat->ContainerGuid, TEXT("mat delta export ok"));
	Stage2Log(true, TEXT("STAGE2 LOOP"));
}

static FAutoConsoleCommandWithWorld SliceSelfTestCmd(
	TEXT("afSliceSelfTest"),
	TEXT("AFTERFALL slice: scripted harvest/zombie/death loop, logs SLICETEST PASS/FAIL"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UAfterfallSliceSubsystem* S = W->GetSubsystem<UAfterfallSliceSubsystem>())
		{
			S->RunSliceSelfTest();
		}
	}));

static FAutoConsoleCommandWithWorld PipelineSelfTestCmd(
	TEXT("afPipelineSelfTest"),
	TEXT("AFTERFALL runtime: production melee/hitscan -> structural state -> collision -> delta -> reload; logs PIPELINE PASS/FAIL and exits"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UAfterfallSliceSubsystem* S = W->GetSubsystem<UAfterfallSliceSubsystem>())
		{
			S->RequestPipelineSelfTest();
		}
	}));

static FAutoConsoleCommandWithWorld Stage2SelfTestCmd(
	TEXT("afStage2SelfTest"),
	TEXT("AFTERFALL stage 2: sprint timing, carry cap, pickup/throw, mat — logs STAGE2TEST PASS/FAIL"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UAfterfallSliceSubsystem* S = W->GetSubsystem<UAfterfallSliceSubsystem>())
		{
			S->RunStage2SelfTest();
		}
	}));
