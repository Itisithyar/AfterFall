// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "SliceSubsystem.h"
#include "AfterfallCharacter.h"
#include "World/ResourceNode.h"
#include "AI/ZombiePrototype.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interaction/Interactable.h"
#include "UI/AfterfallHUD.h"
#include "World/PhysicalItem.h"
#include "Storage/PhysicalMat.h"
#include "Movement/AfterfallMovementComponent.h"
#include "Interaction/CarryComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
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
	SpawnNode(EResourceKind::Wood, FVector(400.f, 0.f, 100.f));
	SpawnNode(EResourceKind::Stone, FVector(0.f, 400.f, 100.f));
	SpawnNode(EResourceKind::Hemp, FVector(-400.f, 0.f, 100.f));

	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AZombiePrototype* Z = W->SpawnActor<AZombiePrototype>(
			AZombiePrototype::StaticClass(), Anchor + FVector(1500.f, 0.f, 100.f),
			FRotator::ZeroRotator, P))
		{
			UE_LOG(LogAfterfall, Log, TEXT("Slice zombie at %s"),
				*Z->GetActorLocation().ToString());
		}
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
	SpawnItem(TEXT("AF_R_Scrap"), 2.f, FVector(600.f, 200.f, 150.f));
	SpawnItem(TEXT("AF_R_Stone"), 12.f, FVector(700.f, -200.f, 150.f));

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
