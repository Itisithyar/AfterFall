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
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"

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
	// Local spawn helper: same production path as placement and the slice
	// seed (SpawnActor + ServerInitialize + damage visuals). Kept local so
	// no locked subsystem header needs a signature change (SpawnStructuralPart
	// is private on the slice subsystem).
	Building.Reset();

	FVector Base = FVector(1500.f, 0.f, 300.f);
	if (AActor* Start = UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()))
	{
		Base = Start->GetActorLocation() + FVector(1500.f, 0.f, 100.f);
	}
	AnchorCached = Base;
	const FName Tag = PlaygroundTag();
	auto Note = [&](AActor* A) { TagActor(A); };

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
		A->Destruction->CellExtentCm = 100.f;
		FStructuralSpec Spec;
		Spec.Part = Part;
		Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Part);
		Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Part);
		A->ServerInitialize(Spec, 0);
		A->RefreshDestructionVisual();
		return A;
	};
	// Floor: 2x2 foundations (existing structural spawn path).
	for (int32 ix = 0; ix < 2; ++ix)
	{
		for (int32 iy = 0; iy < 2; ++iy)
		{
			AStructuralActor* F = SpawnPart(EStructuralPart::Foundation,
				Base + FVector(ix * 100.f, iy * 100.f, 0.f), FRotator::ZeroRotator);
			Note(F);
		}
	}
	// Destroyable building: foundation + 2 walls + roof (same production path).
	const FVector B = Base + FVector(400.f, 0.f, 0.f);
	AStructuralActor* BF = SpawnPart(EStructuralPart::Foundation, B, FRotator::ZeroRotator);
	AStructuralActor* W1 = SpawnPart(EStructuralPart::Wall, B + FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
	AStructuralActor* W2 = SpawnPart(EStructuralPart::Wall, B + FVector(100.f, 0.f, 100.f), FRotator(0.f, 90.f, 0.f));
	AStructuralActor* RF = SpawnPart(EStructuralPart::Wall, B + FVector(0.f, 0.f, 200.f), FRotator::ZeroRotator);
	Note(BF); Note(W1); Note(W2); Note(RF);
	if (BF) Building.Add(BF);
	if (W1) Building.Add(W1);
	if (W2) Building.Add(W2);
	if (RF) Building.Add(RF);

	// Resource node + items (server spawns, ItemId assigned like production).
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AResourceNode* N = W->SpawnActor<AResourceNode>(AResourceNode::StaticClass(), Base + FVector(0.f, 300.f, 50.f), FRotator::ZeroRotator, P))
	{
		Note(N);
	}
	auto SpawnItem = [&](const TCHAR* Type, float Mass, const FVector& At)
	{
		if (APhysicalItem* I = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(), At, FRotator::ZeroRotator, P))
		{
			I->ItemType = Type;
			I->MassKg = Mass;
			I->ItemId = FGuid::NewGuid();
			Note(I);
		}
	};
	SpawnItem(TEXT("AF_R_Scrap"), 2.f, Base + FVector(200.f, 200.f, 150.f));
	SpawnItem(TEXT("AF_R_Stone"), 12.f, Base + FVector(300.f, -200.f, 150.f));

	// Zombie presence (prototype AI actor).
	if (AZombiePrototype* Z = W->SpawnActor<AZombiePrototype>(AZombiePrototype::StaticClass(), Base + FVector(800.f, 0.f, 100.f), FRotator::ZeroRotator, P))
	{
		Note(Z);
	}
	UE_LOG(LogAfterfall, Log, TEXT("MECHTEST playground built at %s"), *Base.ToString());
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
	Check(CountTagged(W, PlaygroundTag()) >= 9, TEXT("floor(4)+building(4)+node+items(2)+zombie present"));
	int32 ValidBuilding = 0;
	for (const TWeakObjectPtr<AStructuralActor>& B : Building)
	{
		if (B.IsValid()) ++ValidBuilding;
	}
	Check(ValidBuilding == 4, TEXT("destroyable building has 4 live parts"));

	// Impact + detach on one building wall (existing melee pipeline).
	bool bDetached = false;
	for (const TWeakObjectPtr<AStructuralActor>& B : Building)
	{
		if (!B.IsValid() || B->Spec.Part != EStructuralPart::Wall) continue;
		FDestructionEvent Swing;
		Swing.Origin = B->GetActorLocation();
		Swing.Direction = FVector(0.f, 0.f, -1.f);
		Swing.InitialEnergy = UDestructionPropagationService::AttackEnergy();
		Swing.Initiator = EDestructionInitiator::Attack;
		Swing.MaterialId = UDestructionPropagationService::MaterialForPart(B->Spec.Part);
		if (B->ServerApplyImpact(Swing, UDestructionPropagationService::AttackDamagePerCell(), FGuid::NewGuid()) > 0)
		{
			bDetached = true;
		}
		break;
	}
	Check(bDetached, TEXT("melee impact detaches building cells"));

	// Grapple attach/release on the player pawn (server-direct RPC path).
	AAfterfallCharacter* Pawn = Cast<AAfterfallCharacter>(UGameplayStatics::GetPlayerPawn(W, 0));
	if (!Pawn || !Pawn->Grapple)
	{
		UE_LOG(LogAfterfall, Log, TEXT("MECHTEST SKIP grapple (no player pawn in headless world)"));
	}
	else
	{
		const FVector SavedLoc = Pawn->GetActorLocation();
		const FRotator SavedRot = Pawn->GetActorRotation();
		if (Building.Num() > 0 && Building[0].IsValid())
		{
			const FVector WallLoc = Building[0]->GetActorLocation();
			const FVector Face = (WallLoc - SavedLoc).GetSafeNormal();
			Pawn->SetActorLocation(WallLoc - Face * 600.f + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->SetActorRotation(Face.Rotation());
		}
		// Server context: call the component directly (the RPC body does the
		// same; the RPC itself is private to the character). Authority + dead
		// checks mirror ServerRequestGrapple_Implementation.
		if (Pawn->HasAuthority() && Pawn->Grapple && !(Pawn->Health && Pawn->Health->bIsDead))
		{
			Pawn->Grapple->TryGrapple();
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

	// Repeatable reset: clear destroys everything, rebuild restores counts.
	const int32 Cleared = ClearPlayground();
	Check(Cleared >= 9 && CountTagged(W, PlaygroundTag()) == 0, TEXT("reset clears all tagged actors"));
	Check(EnsurePlayground() && CountTagged(W, PlaygroundTag()) >= 9, TEXT("rebuild restores playground"));

	UE_LOG(LogAfterfall, Log, TEXT("MECHTEST SUMMARY pass=%d fail=%d MECHTEST %s"),
		Pass, Fail, Fail == 0 ? TEXT("PASS") : TEXT("FAIL"));
	if (bAutoExit) FPlatformMisc::RequestExitWithStatus(false, Fail == 0 ? 0 : 1);
	return Fail == 0;
}
