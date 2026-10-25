// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Muse (Mechanics) — dedicated development test area.
//
// Runtime-spawned playground: no .umap is created or touched. Everything is
// tagged "MechPlay" so the area resets repeatably (destroy + rebuild).
// Reuses production paths only: slice structural spawning (ServerInitialize),
// zombie/item/node actor spawns, server-authoritative grapple RPCs, and the
// melee impact pipeline. Dev/headless only; never runs in Shipping builds
// (console + flag paths are editor/test conveniences, inert in Shipping).
#include "Slice/MechPlaygroundSubsystem.h"
#include "Structural/StructuralActor.h"
#include "Structural/StructuralTypes.h"
#include "Structural/ConstructionValidator.h"
#include "Destruction/DestructionPropagation.h"
#include "World/PhysicalItem.h"
#include "World/ResourceNode.h"
#include "AI/ZombiePrototype.h"
#include "AfterfallCharacter.h"
#include "Movement/AfterfallGrappleComponent.h"
#include "Combat/HealthComponent.h"
#include "Afterfall.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	int32 CountTagged(UWorld* W, FName Tag)
	{
		int32 N = 0;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->Tags.Contains(Tag)) ++N;
		}
		return N;
	}

	void TagActor(AActor* A)
	{
		if (A) A->Tags.Add(UMechPlaygroundSubsystem::PlaygroundTag());
	}

	void MechLog(bool bOk, const TCHAR* What)
	{
		UE_LOG(LogAfterfall, Log, TEXT("MECHTEST %s: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), What);
	}
}

void UMechPlaygroundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("afMechSelfTest")))
	{
		if (UWorld* W = GetWorld())
		{
			FTimerHandle H;
			W->GetTimerManager().SetTimer(H, this, &UMechPlaygroundSubsystem::RunAutoTest, 2.0f, false);
		}
	}
}

void UMechPlaygroundSubsystem::RunAutoTest()
{
	RunMechSelfTest(true);
}

static void RunMechSelfTestCmd(UWorld* W)
{
	if (!W) return;
	if (UMechPlaygroundSubsystem* S = W->GetSubsystem<UMechPlaygroundSubsystem>())
	{
		S->RunMechSelfTest(false);
	}
}

static FAutoConsoleCommandWithWorld GMechSelfTestCmd(
	TEXT("afMechSelfTest"),
	TEXT("AFTERFALL dev: build mechanics playground, assert systems, impact + grapple checks, reset. Headless: -afMechSelfTest on server."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunMechSelfTestCmd));

bool UMechPlaygroundSubsystem::EnsurePlayground()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return false; // server-authoritative: clients only receive replicas
	}
	ClearPlayground();
	Building.Reset();
	StructuralPartCount = 0;
	PropCount = 0;
	BuildingCount = 0;
	FirstWindow = nullptr;

	FVector Base = FVector(0.f, 0.f, 100.f);
	if (AActor* Start = UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()))
	{
		Base = Start->GetActorLocation();
	}
	AnchorCached = Base;
	auto Note = [&](AActor* A) { TagActor(A); };

	// 100cm cells -> one structural part covers 300cm. This is NOT free to
	// raise: melee propagation reach is 120cm (InitiatorReachCm), so a part
	// whose cells are 150cm away from the cursor can never be broken by attack.
	const float CellExtent = 100.f;
	const float Span = CellExtent * 3.f;

	auto SpawnPart = [&](EStructuralPart Part, const FVector& At, const FRotator& Rot)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AStructuralActor* A = W->SpawnActor<AStructuralActor>(
			AStructuralActor::StaticClass(), At, Rot, SP);
		if (!A || !A->Destruction)
		{
			return (AStructuralActor*)nullptr;
		}
		A->Destruction->CellExtentCm = CellExtent;
		FStructuralSpec Spec;
		Spec.Part = Part;
		Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Part);
		Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Part);
		A->ServerInitialize(Spec, 0);
		A->RefreshDestructionVisual();
		Note(A);
		++StructuralPartCount;
		return A;
	};

	// ---- ground, road, props (static; no runtime mobility changes) --------
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube)
	{
		auto Block = [&](const FVector& At, const FVector& SizeCm, const FLinearColor& Color)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(), At, FRotator::ZeroRotator, SP);
			if (!A)
			{
				return;
			}
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			// Movable while the mesh is assigned, Static afterwards: assigning a
			// mesh to a Static component is what produces the mobility warning.
			M->SetMobility(EComponentMobility::Movable);
			M->SetStaticMesh(Cube);
			M->SetMobility(EComponentMobility::Static);
			M->SetWorldScale3D(SizeCm / 100.f);
			if (UMaterialInstanceDynamic* Dyn = M->CreateAndSetMaterialInstanceDynamic(0))
			{
				Dyn->SetVectorParameterValue(TEXT("Color"), Color);
			}
			Note(A);
			++PropCount;
		};
		Block(Base + FVector(0.f, 0.f, -260.f), FVector(24000.f, 24000.f, 500.f),
			FLinearColor(0.06f, 0.07f, 0.06f));            // ground
		Block(Base + FVector(0.f, 0.f, -5.f), FVector(18000.f, 900.f, 10.f),
			FLinearColor(0.05f, 0.05f, 0.055f));           // road
		Block(Base + FVector(0.f, 700.f, 0.f), FVector(18000.f, 60.f, 8.f),
			FLinearColor(0.16f, 0.15f, 0.12f));            // kerb
		Block(Base + FVector(-600.f, 600.f, 40.f), FVector(120.f, 120.f, 80.f),
			FLinearColor(0.22f, 0.14f, 0.07f));            // crate
		Block(Base + FVector(-500.f, 750.f, 60.f), FVector(90.f, 90.f, 120.f),
			FLinearColor(0.20f, 0.16f, 0.09f));            // crate
		Block(Base + FVector(2600.f, -600.f, 45.f), FVector(150.f, 150.f, 90.f),
			FLinearColor(0.19f, 0.19f, 0.20f));            // container
		for (int32 i = 0; i < 6; ++i)
		{
			Block(Base + FVector(-1500.f + i * 260.f, 1100.f, 60.f),
				FVector(240.f, 12.f, 120.f), FLinearColor(0.12f, 0.12f, 0.10f)); // fence
		}
		for (int32 i = 0; i < 4; ++i)
		{
			Block(Base + FVector(-3600.f + i * 200.f, -500.f + i * 160.f, 25.f),
				FVector(180.f, 90.f, 50.f), FLinearColor(0.14f, 0.12f, 0.11f));   // debris
		}
	}

	// ---- three buildings: shells are breakable structural parts -----------
	auto MakeBuilding = [&](const FVector& Origin, float SizeX, float SizeY,
		bool bDamaged, float DoorSideSign)
	{
		++BuildingCount;
		const int32 NX = FMath::Max(2, FMath::RoundToInt(SizeX / Span));
		const int32 NY = FMath::Max(2, FMath::RoundToInt(SizeY / Span));
		const float HX = NX * Span * 0.5f;
		const float HY = NY * Span * 0.5f;

		for (int32 ix = 0; ix < NX; ++ix)
		{
			for (int32 iy = 0; iy < NY; ++iy)
			{
				const FVector P = Origin + FVector(-HX + Span * (ix + 0.5f),
					-HY + Span * (iy + 0.5f), CellExtent * 0.5f);
				if (AStructuralActor* F = SpawnPart(EStructuralPart::Foundation, P,
					FRotator::ZeroRotator))
				{
					Building.Add(F);
				}
				if (AStructuralActor* R = SpawnPart(EStructuralPart::Roof,
					P + FVector(0.f, 0.f, Span), FRotator::ZeroRotator))
				{
					Building.Add(R);
				}
			}
		}

		// Entrance gap in the door-side wall; outer bays of the long walls are
		// Window parts (low durability) so glass breaks differently to walls.
		const int32 MidX = NX / 2;
		const int32 MidY = NY / 2;
		for (int32 ix = 0; ix < NX; ++ix)
		{
			for (int32 side = 0; side < 2; ++side)
			{
				const float Y = (side == 0 ? -HY : HY);
				if (side == 0 && ix == MidX)
				{
					continue; // doorway
				}
				if (bDamaged && ((ix + side) % 3 == 0))
				{
					continue; // ruin: collapsed bay
				}
				const EStructuralPart P = (ix == 0 || ix == NX - 1)
					? EStructuralPart::Window : EStructuralPart::Wall;
				if (AStructuralActor* S = SpawnPart(P,
					Origin + FVector(-HX + Span * (ix + 0.5f), Y, CellExtent * 1.5f),
					FRotator::ZeroRotator))
				{
					Building.Add(S);
					if (P == EStructuralPart::Window && !FirstWindow.IsValid())
					{
						FirstWindow = S;
					}
				}
			}
		}
		for (int32 iy = 0; iy < NY; ++iy)
		{
			for (int32 side = 0; side < 2; ++side)
			{
				const float X = (side == 0 ? -HX : HX);
				if (bDamaged && ((iy + side) % 4 == 0))
				{
					continue;
				}
				const EStructuralPart P = (iy == MidY)
					? EStructuralPart::Window : EStructuralPart::Wall;
				if (AStructuralActor* S = SpawnPart(P,
					Origin + FVector(X, -HY + Span * (iy + 0.5f), CellExtent * 1.5f),
					FRotator(0.f, 90.f, 0.f)))
				{
					Building.Add(S);
					if (P == EStructuralPart::Window && !FirstWindow.IsValid())
					{
						FirstWindow = S;
					}
				}
			}
		}

		if (AStructuralActor* D = SpawnPart(EStructuralPart::Door,
			Origin + FVector(0.f, DoorSideSign * HY, CellExtent * 1.5f),
			FRotator::ZeroRotator))
		{
			Building.Add(D);
		}
		UE_LOG(LogAfterfall, Log, TEXT("PLAYGROUND building at %s (%dx%d parts%s)"),
			*Origin.ToString(), NX, NY, bDamaged ? TEXT(", damaged") : TEXT(""));
	};

	MakeBuilding(Base + FVector(0.f, 2600.f, 0.f), 1350.f, 900.f, false, 1.f);       // residence
	MakeBuilding(Base + FVector(3400.f, 400.f, 0.f), 1800.f, 1350.f, false, -1.f);   // service
	MakeBuilding(Base + FVector(-3400.f, 300.f, 0.f), 1700.f, 1100.f, true, 1.f);    // industrial ruin

	// ---- lighting / atmosphere --------------------------------------------
	{
		FActorSpawnParameters LP;
		LP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(
			ADirectionalLight::StaticClass(), Base + FVector(0.f, 0.f, 1200.f),
			FRotator(-48.f, 35.f, 0.f), LP))
		{
			if (ULightComponent* L = Sun->GetLightComponent())
			{
				L->SetIntensity(3.2f);
			}
			Note(Sun);
		}
		if (ASkyLight* Sky = W->SpawnActor<ASkyLight>(ASkyLight::StaticClass(),
			Base + FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, LP))
		{
			if (USkyLightComponent* C = Sky->GetLightComponent())
			{
				C->SetMobility(EComponentMobility::Movable);
				C->SetIntensity(1.1f);
			}
			Note(Sky);
		}
		if (AExponentialHeightFog* Fog = W->SpawnActor<AExponentialHeightFog>(
			AExponentialHeightFog::StaticClass(), Base + FVector(0.f, 0.f, 200.f),
			FRotator::ZeroRotator, LP))
		{
			if (UExponentialHeightFogComponent* C = Fog->GetComponent())
			{
				C->SetFogDensity(0.02f);
				C->SetFogInscatteringColor(FLinearColor(0.35f, 0.38f, 0.42f));
			}
			Note(Fog);
		}
	}

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AResourceNode* N = W->SpawnActor<AResourceNode>(AResourceNode::StaticClass(),
		Base + FVector(900.f, 900.f, 50.f), FRotator::ZeroRotator, P))
	{
		Note(N);
	}
	auto SpawnItem = [&](const TCHAR* Type, float Mass, const FVector& At)
	{
		if (APhysicalItem* I = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(), At,
			FRotator::ZeroRotator, P))
		{
			I->ItemType = Type;
			I->MassKg = Mass;
			I->ItemId = FGuid::NewGuid();
			Note(I);
		}
	};
	SpawnItem(TEXT("AF_R_Scrap"), 2.f, Base + FVector(600.f, 600.f, 150.f));
	SpawnItem(TEXT("AF_R_Stone"), 12.f, Base + FVector(700.f, -600.f, 150.f));

	if (AZombiePrototype* Z = W->SpawnActor<AZombiePrototype>(AZombiePrototype::StaticClass(),
		Base + FVector(1800.f, 0.f, 100.f), FRotator::ZeroRotator, P))
	{
		Note(Z);
	}
	UE_LOG(LogAfterfall, Log,
		TEXT("PLAYGROUND built at %s: buildings=%d parts=%d props=%d"),
		*Base.ToString(), BuildingCount, StructuralPartCount, PropCount);
	return true;
}

int32 UMechPlaygroundSubsystem::ClearPlayground()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return 0;
	}
	TArray<AActor*> Dead;
	const FName Tag = PlaygroundTag();
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->Tags.Contains(Tag))
		{
			Dead.Add(*It);
		}
	}
	for (AActor* A : Dead)
	{
		A->Destroy();
	}
	Building.Reset();
	return Dead.Num();
}

bool UMechPlaygroundSubsystem::RunMechSelfTest(bool bAutoExit)
{
	UWorld* W = GetWorld();
	int32 Pass = 0, Fail = 0;
	auto Check = [&](bool bOk, const TCHAR* What)
	{
		MechLog(bOk, What);
		if (bOk) ++Pass; else ++Fail;
	};
	if (!W || W->IsNetMode(NM_Client))
	{
		Check(false, TEXT("fixture requires server (not client)"));
		UE_LOG(LogAfterfall, Warning, TEXT("MECHTEST SUMMARY pass=%d fail=%d MECHTEST FAIL"), Pass, Fail);
		if (bAutoExit) FPlatformMisc::RequestExitWithStatus(false, 1);
		return false;
	}

	// Build + counts.
	Check(EnsurePlayground(), TEXT("playground builds on server"));
	Check(CountTagged(W, PlaygroundTag()) >= 40,
		TEXT("ground/road/props + 3 buildings + node/items/zombie present"));
	int32 ValidBuilding = 0;
	for (const TWeakObjectPtr<AStructuralActor>& B : Building)
	{
		if (B.IsValid()) ++ValidBuilding;
	}
	Check(BuildingCount == 3, TEXT("three distinct buildings built"));
	Check(ValidBuilding >= 20, TEXT("building shells have >=20 live structural parts"));
	Check(StructuralPartCount >= 20 && PropCount >= 10,
		TEXT("part + prop counts recorded"));

	// Impact + detach on one building wall (existing melee pipeline).
	bool bDetached = false;
	AStructuralActor* BrokenWall = nullptr;
	for (const TWeakObjectPtr<AStructuralActor>& B : Building)
	{
		if (!B.IsValid() || B->Spec.Part != EStructuralPart::Wall) continue;
		FDestructionEvent Swing;
		// Start at the top surface, like a real trace hit: propagation walks
		// inward by one cell extent per step, so a center-origin event is out
		// of attack reach on large-cell parts.
		Swing.Origin = B->GetActorLocation();
		Swing.Direction = FVector(0.f, 0.f, -1.f);
		Swing.InitialEnergy = UDestructionPropagationService::AttackEnergy();
		Swing.Initiator = EDestructionInitiator::Attack;
		Swing.MaterialId = UDestructionPropagationService::MaterialForPart(B->Spec.Part);
		if (B->ServerApplyImpact(Swing, UDestructionPropagationService::AttackDamagePerCell(), FGuid::NewGuid()) > 0)
		{
			bDetached = true;
			BrokenWall = B.Get();
		}
		break;
	}
	Check(bDetached, TEXT("melee impact detaches building cells"));

	// Glass: a Window part must break at its own (lower) durability.
	bool bGlassBroken = false;
	if (FirstWindow.IsValid())
	{
		FDestructionEvent GlassHit;
		GlassHit.Origin = FirstWindow->GetActorLocation();
		GlassHit.Direction = FVector(0.f, 0.f, -1.f);
		GlassHit.InitialEnergy = UDestructionPropagationService::AttackEnergy();
		GlassHit.Initiator = EDestructionInitiator::Attack;
		GlassHit.MaterialId = UDestructionPropagationService::MaterialForPart(FirstWindow->Spec.Part);
		bGlassBroken = FirstWindow->ServerApplyImpact(GlassHit,
			UDestructionPropagationService::AttackDamagePerCell(), FGuid::NewGuid()) > 0;
	}
	Check(bGlassBroken, TEXT("window/glass part breaks from an impact"));

	// Grapple attach/release on the player pawn (server-direct RPC path).
	AAfterfallCharacter* Pawn = Cast<AAfterfallCharacter>(UGameplayStatics::GetPlayerPawn(W, 0));
	if (!Pawn && W->GetNetMode() != NM_Client)
	{
		// Headless -server has no login, so there is no player pawn at all.
		// Standing one up here (controller + character, both production
		// classes) is what makes these assertions actually run instead of
		// silently skipping; they are tagged separately so the playground
		// reset does not delete the test subject.
		FActorSpawnParameters PSP;
		PSP.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector SpawnAt = AnchorCached + FVector(0.f, 0.f, 200.f);
		if (APlayerController* PC = W->SpawnActor<APlayerController>(
			APlayerController::StaticClass(), SpawnAt, FRotator::ZeroRotator, PSP))
		{
			PC->Tags.Add(TEXT("MechPlayTestPawn"));
			Pawn = W->SpawnActor<AAfterfallCharacter>(
				AAfterfallCharacter::StaticClass(), SpawnAt, FRotator::ZeroRotator, PSP);
			if (Pawn)
			{
				Pawn->Tags.Add(TEXT("MechPlayTestPawn"));
				PC->Possess(Pawn);
			}
		}
		if (!Pawn)
		{
			Check(false, TEXT("grapple fixture has a server pawn to attach with"));
		}
	}
	if (!Pawn || !Pawn->Grapple)
	{
		Check(false, TEXT("grapple attaches to building wall"));
		Check(false, TEXT("rope length assigned"));
	}
	else
	{
		const FVector SavedLoc = Pawn->GetActorLocation();
		const FRotator SavedRot = Pawn->GetActorRotation();
		AStructuralActor* AnchorPart = nullptr;
		for (const TWeakObjectPtr<AStructuralActor>& B : Building)
		{
			// Prefer an intact wall: the impact test already detached cells
			// from BrokenWall, so its surface is no longer where it was.
			if (B.IsValid() && B->Spec.Part == EStructuralPart::Wall && B.Get() != BrokenWall)
			{
				AnchorPart = B.Get();
				break;
			}
		}
		if (!AnchorPart)
		{
			for (const TWeakObjectPtr<AStructuralActor>& B : Building)
			{
				if (B.IsValid() && B->Spec.Part == EStructuralPart::Wall)
				{
					AnchorPart = B.Get();
					break;
				}
			}
		}
		if (AnchorPart)
		{
			const FVector WallLoc = AnchorPart->GetActorLocation();
			const FVector Face = (WallLoc - SavedLoc).GetSafeNormal();
			// Same height as the wall centre: a +100cm offset put the camera
			// above the 300cm wall and the grapple ray over the top.
			Pawn->SetActorLocation(WallLoc - Face * 450.f, false, nullptr, ETeleportType::TeleportPhysics);
			// Yaw only, recomputed AFTER the teleport: the grapple traces along the
			// pawn's actor forward vector (not the camera), so a diagonal/3D facing
			// vector aims the ray past the wall.
			const FVector FaceFlat = (WallLoc - Pawn->GetActorLocation()).GetSafeNormal2D();
			Pawn->SetActorRotation(FRotator(0.f, FaceFlat.Rotation().Yaw, 0.f));
		}
		// Server context: call the component directly (the RPC body does the
		// same; the RPC itself is private to the character). Authority + dead
		// checks mirror ServerRequestGrapple_Implementation.
		if (Pawn->HasAuthority() && Pawn->Grapple && !(Pawn->Health && Pawn->Health->bIsDead))
		{
			Pawn->Grapple->TryGrapple();
		}
		// Diagnostic: the grapple traces along the pawn's actor forward vector, so
		// log exactly where that ray goes and what it hits.
		{
			FHitResult Probe;
			FCollisionQueryParams QP(SCENE_QUERY_STAT(MechGrappleProbe), false, Pawn);
			const FVector From = Pawn->GetActorLocation();
			const bool bHit = W->LineTraceSingleByChannel(Probe, From,
				From + Pawn->GetActorForwardVector() * 2000.f, ECC_Visibility, QP);
			UE_LOG(LogAfterfall, Log,
				TEXT("MECHTEST grapple probe: pawnloc=%s fwd=%s hit=%s at %.0fcm state=%d"),
				*From.ToString(), *Pawn->GetActorForwardVector().ToString(),
				bHit && Probe.GetActor() ? *Probe.GetActor()->GetName() : TEXT("nothing"),
				bHit ? Probe.Distance : -1.f, (int32)Pawn->Grapple->State);
		}
		const bool bAttached = Pawn->Grapple->IsAttached();
		Check(bAttached, TEXT("grapple attaches to building wall"));
		Check(Pawn->Grapple->RopeLength > 0.f, TEXT("rope length assigned"));
		if (bAttached)
		{
			for (int32 i = 0; i < 30; ++i)
			{
				Pawn->Grapple->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
			}
			const float Dist = FVector::Dist(Pawn->GetActorLocation(), Pawn->Grapple->Anchor);
			Check(Dist <= Pawn->Grapple->RopeLength + Pawn->Grapple->Tuning.ConstraintToleranceCm + 1.f,
				TEXT("rope constraint respected after ticks"));
			const FVector VelBefore = Pawn->GetVelocity();
			Pawn->Grapple->Release();
			Check(!Pawn->Grapple->IsAttached(), TEXT("release detaches"));
			Check(Pawn->GetVelocity().Equals(VelBefore), TEXT("release preserves momentum"));
		}
		Pawn->SetActorLocation(SavedLoc, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->SetActorRotation(SavedRot);
	}

	// Remove the stand-in pawn the headless fixture created (never the real
	// player's pawn: only our spawn is tagged).
	{
		TArray<AActor*> StandIns;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->Tags.Contains(TEXT("MechPlayTestPawn")))
			{
				StandIns.Add(*It);
			}
		}
		for (AActor* A : StandIns)
		{
			A->Destroy();
		}
	}

	// Repeatable reset: clear destroys everything, rebuild restores counts.
	const int32 Cleared = ClearPlayground();
	Check(Cleared >= 9 && CountTagged(W, PlaygroundTag()) == 0, TEXT("reset clears all tagged actors"));
	Check(EnsurePlayground() && CountTagged(W, PlaygroundTag()) >= 9, TEXT("rebuild restores playground"));

	UE_LOG(LogAfterfall, Log, TEXT("MECHTEST SUMMARY pass=%d fail=%d MECHTEST %s"),
		Pass, Fail, Fail == 0 ? TEXT("PASS") : TEXT("FAIL"));
	if (bAutoExit) FPlatformMisc::RequestExitWithStatus(false, Fail == 0 ? 0 : 1);
	return Fail == 0;
}
