// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "PhysicsValidationSubsystem.h"
#include "AfterfallCharacter.h"
#include "World/PhysicalItem.h"
#include "Interaction/CarryComponent.h"
#include "Movement/AfterfallMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "UObject/ConstructorHelpers.h"
#include "Afterfall.h"

namespace
{
// Rig origin: far from authored content, floor top exactly Z=0.
constexpr float RigX = 5000.f;
constexpr float TestMassKg = 5.f;
constexpr float FallSampleSec = 1.0;
// NOTE: the task's headline numbers (d=490.5cm, v=980cm/s at t=1s) are the
// ideal-model values; the fixture predicts against physics time, which
// equals game time only when every frame fits MaxPhysicsDeltaTime.
constexpr float BodyHalfCm = 50.f; // unit Cube is 100cm

void PhyLog(bool bOk, const TCHAR* Name, const FString& Detail)
{
	UE_LOG(LogAfterfall, Log, TEXT("PHYTEST %s %s (%s)"),
		bOk ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
}
} // namespace

bool UPhysicsValidationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
 return Cast<UWorld>(Outer) != nullptr;
}

void UPhysicsValidationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("afPhysicsSelfTest")))
	{
		if (UWorld* W = GetWorld())
		{
			FTimerHandle H;
			W->GetTimerManager().SetTimer(H, this,
				&UPhysicsValidationSubsystem::RunPhysicsSelfTest, 1.0f, false);
		}
	}
}

void UPhysicsValidationSubsystem::Deinitialize()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(LoopTimer);
	}
	Super::Deinitialize();
}

void UPhysicsValidationSubsystem::Check(bool bOk, const TCHAR* Name, const FString& Detail)
{
	if (bOk)
	{
		++PassCount;
	}
	else
	{
		++FailCount;
	}
	PhyLog(bOk, Name, Detail);
}

void UPhysicsValidationSubsystem::RunPhysicsSelfTest()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		UE_LOG(LogAfterfall, Error, TEXT("PHYTEST FAIL server-only"));
		FPlatformMisc::RequestExitWithStatus(true, 1);
		return;
	}
	Step = 0;
	PassCount = 0;
	FailCount = 0;
	TotalT = 0.0;
	TotalFrames = 0;
	bPawnModeSet = false;
	TestStartT = W->TimeSeconds;
	MaxPhysDt = 1.f / 30.f;
	if (const UPhysicsSettings* PS = UPhysicsSettings::Get())
	{
		MaxPhysDt = PS->MaxPhysicsDeltaTime;
	}
	UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG maxphysdt=%.4f"), MaxPhysDt);
	// Fire every frame (interval << frame time): per-fire DeltaTimeSeconds
	// is then exactly one frame, so capped accumulation equals solver time.
	W->GetTimerManager().SetTimer(LoopTimer, this,
		&UPhysicsValidationSubsystem::StepTest, 0.005f, true);
}

UStaticMeshComponent* UPhysicsValidationSubsystem::SpawnBody(const FVector& At)
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Body = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), At,
		FRotator::ZeroRotator, P);
	if (!Body)
	{
		return nullptr;
	}
	Body->SetReplicates(false);
	Body->SetReplicateMovement(false);
	UStaticMeshComponent* C = Body->GetStaticMeshComponent();
	if (!C)
	{
		return nullptr;
	}
	// Configure off-registration: the engine rejects mesh swaps on
	// registered static-mobility components, and this also batches the
	// physics (re)creation into a single valid state.
	C->UnregisterComponent();
	C->SetStaticMesh(UnitCube);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionProfileName(TEXT("PhysicsActor"));
	C->SetSimulatePhysics(true);
	C->SetEnableGravity(true);
	C->SetMassOverrideInKg(NAME_None, TestMassKg, true);
	C->SetPhysMaterialOverride(TestMat);
	C->SetLinearDamping(0.0f);
	C->SetAngularDamping(0.0f);
	C->RegisterComponent();
	Check(C->GetStaticMesh() == UnitCube, TEXT("rig body mesh"), TEXT("cube attached"));
	if (C->GetStaticMesh() == nullptr)
	{
		return nullptr;
	}
	return C;
}

void UPhysicsValidationSubsystem::SetupRig()
{
	UWorld* W = GetWorld();
	TestMat = NewObject<UPhysicalMaterial>(this);
	TestMat->Friction = 0.8f;
	TestMat->Restitution = 0.2f;
	// FObjectFinder is constructor-only; at runtime load via LoadObject.
	if (!UnitCube)
	{
		UnitCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	Check(UnitCube != nullptr, TEXT("rig mesh load"), TEXT("Engine Cube"));
	if (!UnitCube)
	{
		FinishTest();
		return;
	}
	if (!UnitCube)
	{
		Check(false, TEXT("rig"), TEXT("unit cube load failed"));
		FinishTest();
		return;
	}

	// Floor: 50m x 50m x 1m block, top surface exactly Z=0.
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Floor = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
		FVector(RigX, 0.f, -50.f), FRotator::ZeroRotator, P);
	if (Floor)
	{
		Floor->SetReplicates(false);
		Floor->SetReplicateMovement(false);
		if (UStaticMeshComponent* F = Floor->GetStaticMeshComponent())
		{		F->UnregisterComponent();
		F->SetStaticMesh(UnitCube);
		F->SetMobility(EComponentMobility::Static);
			F->SetWorldScale3D(FVector(50.f, 50.f, 1.f));
		F->SetCollisionProfileName(TEXT("BlockAll"));
			F->SetSimulatePhysics(false);
			F->SetPhysMaterialOverride(TestMat);
		F->RegisterComponent();
		}
	}

	BodyMesh = SpawnBody(FVector(RigX, 0.f, 1300.f));
}

void UPhysicsValidationSubsystem::StepTest()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Dt = W->DeltaTimeSeconds;
	TotalT = W->TimeSeconds - TestStartT;
	++TotalFrames;
	if (TotalT > 180.0)
	{
		Check(false, TEXT("watchdog"), TEXT("180s exceeded"));
		FinishTest();
		return;
	}

	switch (Step)
	{
	case 0:
	{
		SetupRig();
		if (!Floor || !BodyMesh)
		{
			Check(false, TEXT("rig"), TEXT("spawn failed"));
			FinishTest();
			return;
		}
		UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG world=%s begunplay=%d netmode=%d t=%.2f"),
			*W->GetName(), (int32)W->HasBegunPlay(), (int32)W->GetNetMode(), W->TimeSeconds);
		UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG body begunplay=%d tick=%d sim=%d state=%d"),
			(int32)Body->HasActorBegunPlay(), (int32)Body->IsActorTickEnabled(),
			(int32)BodyMesh->IsSimulatingPhysics(), (int32)BodyMesh->IsPhysicsStateCreated());
		UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG mesh reg=%d collision=%d profile=%s scene=%d validbody=%dOwningWorld=%s"),
			(int32)BodyMesh->IsRegistered(), (int32)BodyMesh->GetCollisionEnabled(),
			*BodyMesh->GetCollisionProfileName().ToString(),
			(int32)(BodyMesh->GetWorld() && BodyMesh->GetWorld()->GetPhysicsScene() != nullptr),
			(int32)(BodyMesh->GetBodyInstance() && BodyMesh->GetBodyInstance()->IsValidBodyInstance()),
			BodyMesh->GetWorld() ? *BodyMesh->GetWorld()->GetName() : TEXT("null"));
		UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG rawflag=%d mesh=%d bodysetup=%d boxes=%d"),
			(int32)BodyMesh->BodyInstance.bSimulatePhysics,
			(int32)(BodyMesh->GetStaticMesh() != nullptr),
			(int32)(BodyMesh->GetStaticMesh() && BodyMesh->GetStaticMesh()->GetBodySetup() != nullptr),
			(BodyMesh->GetStaticMesh() && BodyMesh->GetStaticMesh()->GetBodySetup()) ?
				BodyMesh->GetStaticMesh()->GetBodySetup()->AggGeom.BoxElems.Num() : -1);
		BodyMesh->RecreatePhysicsState();
		UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG after recreate sim=%d state=%d"),
			(int32)BodyMesh->IsSimulatingPhysics(), (int32)BodyMesh->IsPhysicsStateCreated());
		// Probe: the project's own gameplay rigid body (constructor-flag
		// pattern, same world) — distinguishes fixture bug from environment.
		FActorSpawnParameters PP;
		PP.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (APhysicalItem* Probe = W->SpawnActor<APhysicalItem>(
			APhysicalItem::StaticClass(), FVector(RigX + 600.f, 0.f, 1300.f),
			FRotator::ZeroRotator, PP))
		{
			if (UPrimitiveComponent* PMC = Cast<UPrimitiveComponent>(Probe->GetRootComponent()))
			{
				UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG probe item sim=%d state=%d"),
					(int32)PMC->IsSimulatingPhysics(), (int32)PMC->IsPhysicsStateCreated());
			}
			Probe->Destroy();
		}
		Check(W->HasBegunPlay(), TEXT("world begun play"), W->GetName());
		Check(BodyMesh->IsPhysicsStateCreated(), TEXT("physics state created"),
			FString::Printf(TEXT("sim=%d"), (int32)BodyMesh->IsSimulatingPhysics()));
		if (!W->HasBegunPlay() || !BodyMesh->IsPhysicsStateCreated())
		{
			FinishTest();
			return;
		}
		const double G = (double)FMath::Abs(W->GetGravityZ());
		Check(FMath::IsNearlyEqual(G, 980.0, 1.0), TEXT("gravity setting"),
			FString::Printf(TEXT("GravityZ=%.1f expect 980"), G));
		Check(FMath::IsNearlyEqual((double)BodyMesh->GetMass(), (double)TestMassKg, 0.01),
			TEXT("mass override"),
			FString::Printf(TEXT("mass=%.3fkg expect %.1f"), BodyMesh->GetMass(), TestMassKg));
		BodyMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		BodyMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		RecZ0 = Body->GetActorLocation().Z;
		PhaseT = 0.0;
		PhaseFrames = 0;
		Step = 1;
		break;
	}
	case 1:
	{
		// Free fall: accumulate 1.0s of physics time, all of it airborne
		// (spawn 1300, fall 490 -> still ~800 above the floor). Physics
		// time drops game-time on slow frames (Chaos clamps each frame to
		// the project's MaxPhysicsDeltaTime): accumulate the same capped
		// clock the solver sees and predict against THAT.
		PhaseT += FMath::Min((double)Dt, (double)MaxPhysDt);
		++PhaseFrames;
		if (PhaseT >= FallSampleSec)
		{
		const double Z1 = Body->GetActorLocation().Z;
		const double D = RecZ0 - Z1;
		const FVector V = BodyMesh->GetPhysicsLinearVelocity();
		const double PredD = 0.5 * 980.0 * PhaseT * PhaseT;
		const double PredV = 980.0 * PhaseT;
		const double AvgDt = TotalT / FMath::Max(TotalFrames, 1);
		Check(FMath::Abs(D - PredD) <= 0.08 * PredD + 5.0, TEXT("free fall d=1/2gt^2"),
			FString::Printf(TEXT("d=%.1fcm expect %.1f physT=%.3fs avgdt=%.4f"),
				D, PredD, PhaseT, AvgDt));
		Check(FMath::Abs(-V.Z - PredV) <= 0.08 * PredV + 10.0, TEXT("free fall v=g*t"),
			FString::Printf(TEXT("Vz=%.1f expect -%.1f"), V.Z, PredV));
			// Impulse phase: stop in place (still airborne), then kick +X.
			BodyMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			BodyMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			BodyMesh->AddImpulse(FVector(TestMassKg * 300.f, 0.f, 0.f), NAME_None, false);
			Step = 2;
		}
		break;
	}
	case 2:
	{
		// J = m*dv: 5kg * 300cm/s = 1500. Read before gravity contaminates X.
		const FVector V = BodyMesh->GetPhysicsLinearVelocity();
		Check(FMath::Abs(V.X - 300.0) <= 30.0, TEXT("impulse J=m*dv"),
			FString::Printf(TEXT("Vx=%.1f expect 300 (J=1500, m=5kg)"), V.X));
		Check(FMath::Abs(V.Y) <= 10.0, TEXT("impulse no cross-axis"),
			FString::Printf(TEXT("Vy=%.2f"), V.Y));
		Body->SetActorLocation(FVector(RigX + 200.f, 0.f, 300.f), false, nullptr,
			ETeleportType::TeleportPhysics);
		BodyMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		BodyMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		MinZ = 1e9;
		QuietT = 0.0;
		PhaseT = 0.0;
		Step = 3;
		break;
	}
	case 3:
	{
		// Settle: drop 250cm, wait for rest on the floor top (Z=50).
		PhaseT += Dt;
		const double Z = Body->GetActorLocation().Z;
		MinZ = FMath::Min(MinZ, Z);
		const double Speed = BodyMesh->GetPhysicsLinearVelocity().Size();
		if (Speed < 12.0)
		{
			QuietT += Dt;
		}
		else
		{
			QuietT = 0.0;
		}
		if (QuietT >= 0.5)
		{
			Check(FMath::Abs(Z - BodyHalfCm) <= 3.0, TEXT("contact rest height"),
				FString::Printf(TEXT("Z=%.2f expect 50 (floor top 0 + half 50)"), Z));
			Check(MinZ >= (double)BodyHalfCm - 2.0, TEXT("no penetration"),
				FString::Printf(TEXT("minZ=%.2f"), MinZ));
			Check(Speed < 12.0, TEXT("settled"),
				FString::Printf(TEXT("speed=%.2f"), Speed));
			// Friction phase: shove +X along the floor.
			BodyMesh->SetPhysicsLinearVelocity(FVector(400.f, 0.f, 0.f));
			RecX0 = Body->GetActorLocation().X;
			PhaseT = 0.0;
			Step = 4;
		}
		else if (PhaseT > 8.0)
		{
			Check(false, TEXT("settle"), TEXT("never came to rest"));
			FinishTest();
		}
		break;
	}
	case 4:
	{
		// Friction: mu=0.8 -> decel 784cm/s^2, stop from 400 in ~0.51s/~102cm.
		PhaseT += Dt;
		if (PhaseT >= 1.5)
		{
			const FVector V = BodyMesh->GetPhysicsLinearVelocity();
			const double X1 = Body->GetActorLocation().X;
			Check(V.Size2D() < 30.0, TEXT("friction stops slide"),
				FString::Printf(TEXT("Vxy=%.1f (mu=0.8)"), V.Size2D()));
			Check((X1 - RecX0) > 50.0, TEXT("friction slid not stuck"),
				FString::Printf(TEXT("dx=%.1fcm"), X1 - RecX0));
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			Pawn = W->SpawnActor<AAfterfallCharacter>(AAfterfallCharacter::StaticClass(),
				FVector(RigX + 400.f, 0.f, 200.f), FRotator::ZeroRotator, P);
			if (!Pawn)
			{
				Check(false, TEXT("pawn"), TEXT("spawn failed"));
				FinishTest();
				return;
			}
			PawnT = 0.0;
			Step = 5;
		}
		break;
	}
	case 5:
	{
		// Let the pawn land, then prove the real pickup/throw/drop path.
		PawnT += Dt;
		if (PawnT >= 1.2)
		{
			if (UCharacterMovementComponent* PM = Pawn->GetCharacterMovement())
		{
			// Dedicated-server spawn leaves MovementMode at None (no
				// possession/auto-init in this harness): init explicitly,
				// once, then run a second 1.2s window to fall and land.
				if (!bPawnModeSet)
				{
					bPawnModeSet = true;
					PM->SetMovementMode(MOVE_Walking);
					UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG pawn mode init -> Walking"));
					PawnT = 0.0;
					break;
				}
				UE_LOG(LogAfterfall, Log, TEXT("PHYTEST DIAG pawn ptick=%d mtick=%d mode=%d vel=%s"),
					(int32)Pawn->IsActorTickEnabled(),
					(int32)PM->IsComponentTickEnabled(),					(int32)PM->MovementMode,
				*Pawn->GetVelocity().ToString());
		}
		const double PZ = Pawn->GetActorLocation().Z;
			Check(PZ > 0.0 && PZ < 200.0, TEXT("pawn grounded"),
				FString::Printf(TEXT("pawnZ=%.1f"), PZ));
			Check(Pawn->GetCharacterMovement()->MovementMode == MOVE_Walking,
				TEXT("pawn walking"), TEXT("movement mode"));
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FVector PawnLoc = Pawn->GetActorLocation();
			ItemA = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(),
				PawnLoc + FVector(120.f, 0.f, 0.f), FRotator::ZeroRotator, P);
			ItemB = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(),
				PawnLoc + FVector(-120.f, 0.f, 0.f), FRotator::ZeroRotator, P);
			if (!ItemA || !ItemB || !Pawn->Carry)
			{
				Check(false, TEXT("items"), TEXT("spawn failed"));
				FinishTest();
				return;
			}
			Step = 6;
		}
		break;
	}
	case 6:
	{
		UCarryComponent* Carry = Pawn->Carry;
		Carry->ServerPickup(ItemA);
		Check(Carry->bIsCarrying && Carry->CarriedItem == ItemA &&
			ItemA->WorldState == EItemWorldState::Held,
			TEXT("pickup transacts"), TEXT("Free->Held"));
		Carry->ServerPickup(ItemB); // duplicate: must be rejected
		Check(Carry->CarriedItem == ItemA && ItemB->WorldState == EItemWorldState::Free,
			TEXT("duplicate pickup rejected"), TEXT("still holds A"));
		Carry->ServerThrow(FVector::ZeroVector); // invalid dir: rejected
		Check(Carry->CarriedItem == ItemA, TEXT("zero-dir throw rejected"),
			TEXT("still holds A"));
		const FVector Dir(1.f, 0.f, 0.2f);
		const FVector Expect = APhysicalItem::ComputeReleaseVelocity(
			Pawn->GetVelocity(), Dir, Carry->ThrowSpeedCmS, Carry->ThrowInheritFactor);
		Carry->ServerThrow(Dir);
		Check(!Carry->bIsCarrying && ItemA->WorldState == EItemWorldState::Free,
			TEXT("throw releases"), TEXT("Held->Free"));
		Check(ItemA->LastReleaseVelocity.Equals(Expect, 1.0f),
			TEXT("throw release model"),
			FString::Printf(TEXT("got %s expect %s"),
				*ItemA->LastReleaseVelocity.ToString(), *Expect.ToString()));
		{
			const FVector V = ItemA->GetVelocity();
			Check(FMath::Abs(V.X - Expect.X) <= 10.0 && FMath::Abs(V.Y - Expect.Y) <= 10.0 &&
				FMath::Abs(V.Z - Expect.Z) <= 40.0, TEXT("throw imparts velocity"),
				FString::Printf(TEXT("V=%s expect %s"), *V.ToString(), *Expect.ToString()));
		}
		ItemC = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(),
			Pawn->GetActorLocation() + FVector(120.f, 0.f, 0.f),
			FRotator::ZeroRotator, FActorSpawnParameters());
		if (!ItemC)
		{
			Check(false, TEXT("itemC"), TEXT("spawn failed"));
			FinishTest();
			return;
		}
		Carry->ServerPickup(ItemC);
		Check(Carry->CarriedItem == ItemC, TEXT("re-pickup"), TEXT("holds C"));
		Carry->ServerDrop();
		Check(!Carry->bIsCarrying && ItemC->WorldState == EItemWorldState::Free &&
			ItemC->GetVelocity().Size() < 8.0, TEXT("drop zero-velocity"),
			FString::Printf(TEXT("V=%.1f"), ItemC->GetVelocity().Size()));
		Step = 7;
		break;
	}
	case 7:
	{
		// Control defaults + jump actuation + sprint intent path.
		UCharacterMovementComponent* M = Pawn->GetCharacterMovement();
		Check(FMath::IsNearlyEqual(M->JumpZVelocity, 550.f, 0.5f),
			TEXT("jump default"),
			FString::Printf(TEXT("JumpZ=%.1f"), M->JumpZVelocity));
		Check(FMath::IsNearlyEqual(M->MaxWalkSpeed, 500.f, 0.5f),
			TEXT("walk default"),
			FString::Printf(TEXT("MaxWalk=%.1f"), M->MaxWalkSpeed));
		Check(M->GetNavAgentPropertiesRef().bCanCrouch &&
			FMath::IsNearlyEqual(M->MaxWalkSpeedCrouched, 200.f, 0.5f),
			TEXT("crouch default"), TEXT("can crouch, 200cm/s"));
		Pawn->Jump();
		Check(Pawn->GetVelocity().Z > 300.f, TEXT("jump actuates"),
			FString::Printf(TEXT("Vz=%.1f"), Pawn->GetVelocity().Z));
		if (UAfterfallMovementComponent* AM = Pawn->GetAfterfallMovement())
		{
			AM->SetSprinting(true);
		}
		Step = 8;
		break;
	}
	case 8:
	{
		if (UAfterfallMovementComponent* AM = Pawn->GetAfterfallMovement())
		{
			Check(AM->SprintState == EAfterfallSprintState::Sprinting,
				TEXT("sprint engages"), TEXT("Ready->Sprinting"));
			AM->SetSprinting(false);
		}
		else
		{
			Check(false, TEXT("sprint engages"), TEXT("no movement comp"));
		}
		Step = 9;
		break;
	}
	case 9:
	{
		if (UAfterfallMovementComponent* AM = Pawn->GetAfterfallMovement())
		{
			Check(AM->SprintState == EAfterfallSprintState::Ready,
				TEXT("sprint releases"), TEXT("->Ready, no lockout (early release)"));
		}
		// Input->movement path: AddMovementInput feeds the pending vector
		// exactly as AAfterfallCharacter::Move does per input event.
		Pawn->AddMovementInput(FVector(1.f, 0.f, 0.f), 1.0f);
		Check(Pawn->GetPendingMovementInputVector().Size() > 0.99,
			TEXT("move input feeds"),
			FString::Printf(TEXT("pending=%s"),
				*Pawn->GetPendingMovementInputVector().ToString()));		// Pause: the handler gates on local control (dedicated servers have
		// none, and SetGamePaused needs a player controller to install a
		// pauser), so the toggle itself is a MANUAL in-game acceptance
		// item. The fixture only pins the guard both branches rely on.
		Check(!Pawn->IsLocallyControlled(), TEXT("pause server guard"),
			TEXT("dedicated pawn is not locally controlled"));
		FinishTest();
		break;
	}
	default:
		FinishTest();
		break;
	}
}

void UPhysicsValidationSubsystem::FinishTest()
{
	UWorld* W = GetWorld();
	if (W)
	{
		W->GetTimerManager().ClearTimer(LoopTimer);
	}
	UE_LOG(LogAfterfall, Log, TEXT("PHYTEST SUMMARY pass=%d fail=%d %s"),
		PassCount, FailCount, FailCount == 0 ? TEXT("PHYTEST PASS") : TEXT("PHYTEST FAIL"));
	if (UWorld* W2 = GetWorld())
	{
		if (APlayerController* PC = W2->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("quit"), true);
		}
	}
	FPlatformMisc::RequestExitWithStatus(true, FailCount == 0 ? 0 : 1);
}

static FAutoConsoleCommandWithWorld PhysicsSelfTestCmd(
	TEXT("afPhysicsSelfTest"),
	TEXT("AFTERFALL dev: headless rigid-body acceptance (free-fall/impulse/settle/friction/throw/controls)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W)
		{
			return;
		}
		if (UPhysicsValidationSubsystem* S = W->GetSubsystem<UPhysicsValidationSubsystem>())
		{
			S->RunPhysicsSelfTest();
		}
	}));
