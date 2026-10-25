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
#include "HAL/PlatformTime.h"
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
	LastStepTime = -1.f;
	JumpWaitTicks = 0;
	bPawnModeSet = false;
	TestStartT = W->TimeSeconds;
	WallStartS = FPlatformTime::Seconds();
	MaxPhysDt = 1.f / 30.f;
	if (const UPhysicsSettings* PS = UPhysicsSettings::Get())
	{
		MaxPhysDt = PS->MaxPhysicsDeltaTime;
		// Clock diagnostic: substep config, dilation, pause — all public.
		// Distinguishes game-clock/fast-timer artifacts from solver starvation.
		const AWorldSettings* WS = W->GetWorldSettings();
		UE_LOG(LogAfterfall, Log,
			TEXT("PHYTEST DIAG maxphysdt=%.4f substep=%d maxsubdt=%.4f maxsub=%d dilation=%.3f paused=%d"),
			MaxPhysDt, (int32)PS->bSubstepping, PS->MaxSubstepDeltaTime, PS->MaxSubsteps,
			WS ? WS->TimeDilation : -1.f, (int32)W->IsPaused());
	}
	// Small interval so no tick is ever skipped; StepTest self-throttles to
	// one execution per world tick (see LastStepTime guard), so slow
	// headless ticks can never inflate the physics-time accumulation.
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
	// ROOT-CAUSE FIX (2026-10-24, measured): the 0.005s loop fires several
	// times per world tick whenever a tick outlasts the interval (headless
	// software rendering → ~30ms+ ticks → ~6 fires/tick). Each firing used
	// to accumulate the full tick Dt, so fixture "physics time" ran ~6x
	// faster than the game/physics clock and sampled before integration.
	// Step the fixture at most once per tick: repeat fires within the same
	// tick return early. TimeSeconds is tick-quantized, so equality is exact.
	const float NowT = W->TimeSeconds;
	if (NowT == LastStepTime)
	{
		return;
	}
	LastStepTime = NowT;
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
		const double PredV = 980.0 * PhaseT;			const double AvgDt = TotalT / FMath::Max(TotalFrames, 1);
			LogClocks(TEXT("freefall-sample"));
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
			// Rig pawns are never possessed (no controller in this harness).
			// UE 5.8 PhysWalking/PhFalling early-out and zero velocity for
			// controllerless pawns unless this flag is set — without it the
			// pawn hovers at spawn height forever. Instance-scoped to the rig;
			// real gameplay always possesses (player or AI), so no game impact.
			if (Pawn && Pawn->GetCharacterMovement())
			{
				Pawn->GetCharacterMovement()->bRunPhysicsWithNoController = true;
			}
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
		// Jump() only latches bPressedJump; the movement tick consumes it
		// (DoJump) on a LATER tick, so asserting Vz synchronously here can
		// never pass. Defer the read to step 8 with a bounded wait.
		Pawn->Jump();
		JumpWaitTicks = 0;
		if (UAfterfallMovementComponent* AM = Pawn->GetAfterfallMovement())
		{
			AM->SetSprinting(true);
		}
		Step = 8;
		break;
	}
	case 8:
	{
		// Jump consumption happens in the movement tick, which may run
		// after this step within the tick order: wait boundedly, then read.
		const float JumpVz = Pawn ? Pawn->GetVelocity().Z : 0.f;
		if (JumpVz <= 300.f && JumpWaitTicks < 30)
		{
			++JumpWaitTicks;
			break; // stay in step 8; sprint checks run once jump resolves
		}
		Check(JumpVz > 300.f, TEXT("jump actuates"),
			FString::Printf(TEXT("Vz=%.1f"), JumpVz));
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

void UPhysicsValidationSubsystem::LogClocks(const TCHAR* Tag)
{
	if (UWorld* W = GetWorld())
	{
		const double Wall = FPlatformTime::Seconds() - WallStartS;
		const double Game = W->TimeSeconds - TestStartT;
		UE_LOG(LogAfterfall, Log,
			TEXT("PHYTEST CLOCK %s wall=%.3fs game=%.3fs frames=%d avgdt=%.4f"),
			Tag, Wall, Game, TotalFrames, TotalFrames > 0 ? Game / TotalFrames : 0.0);
	}
}

void UPhysicsValidationSubsystem::FinishTest()
{
	UWorld* W = GetWorld();
	LogClocks(TEXT("finish"));
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

void UPhysicsValidationSubsystem::ToggleLabLogging()
{
	bLabLogging = !bLabLogging;
	LabSummary = bLabLogging ? TEXT("Impact logging ON.") : TEXT("Impact logging OFF.");
	UE_LOG(LogAfterfall, Log, TEXT("PHYLAB logging %s"), bLabLogging ? TEXT("ON") : TEXT("OFF"));
}

void UPhysicsValidationSubsystem::RunPhysicsLab()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PHYLAB server only"));
		return;
	}
	if (bLabRunning || LoopTimer.IsValid())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PHYLAB busy (self-test or lab already running)"));
		return;
	}
	// Lab rig: separate X so it never touches the self-test rig or the room.
	constexpr float LabX = 9000.f;
	constexpr float LabMassKg = 10.f;
	constexpr float LabDropCm = 500.f; // 5m
	if (!UnitCube)
	{
		UnitCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!UnitCube)
	{
		LabSummary = TEXT("Lab FAIL: cube mesh missing.");
		UE_LOG(LogAfterfall, Warning, TEXT("PHYLAB %s"), *LabSummary);
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	LabFloor = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
		FVector(LabX, 0.f, -50.f), FRotator::ZeroRotator, P);
	LabBody = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
		FVector(LabX, 0.f, LabDropCm + 50.f), FRotator::ZeroRotator, P);
	if (!LabFloor || !LabBody)
	{
		LabSummary = TEXT("Lab FAIL: rig spawn failed.");
		FinishLab(false);
		return;
	}
	LabFloor->SetReplicates(false);
	LabBody->SetReplicates(false);
	if (UStaticMeshComponent* F = LabFloor->GetStaticMeshComponent())
	{
		F->UnregisterComponent();
		F->SetStaticMesh(UnitCube);
		F->SetMobility(EComponentMobility::Static);
		F->SetWorldScale3D(FVector(50.f, 50.f, 1.f));
		F->SetCollisionProfileName(TEXT("BlockAll"));
		F->SetSimulatePhysics(false);
		F->RegisterComponent();
	}
	LabMesh = LabBody->GetStaticMeshComponent();
	if (!LabMesh)
	{
		LabSummary = TEXT("Lab FAIL: no mesh component.");
		FinishLab(false);
		return;
	}
	LabMesh->UnregisterComponent();
	LabMesh->SetStaticMesh(UnitCube);
	LabMesh->SetMobility(EComponentMobility::Movable);
	LabMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	LabMesh->SetSimulatePhysics(true);
	LabMesh->SetEnableGravity(true);
	LabMesh->SetMassOverrideInKg(NAME_None, LabMassKg, true);
	LabMesh->SetLinearDamping(0.0f);
	LabMesh->SetAngularDamping(0.0f);
	LabMesh->RegisterComponent();
	LabMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	LabMesh->SetNotifyRigidBodyCollision(true);
	LabMesh->OnComponentHit.AddDynamic(this, &UPhysicsValidationSubsystem::OnLabHit);
	bLabRunning = true;
	LabPhase = 0;
	LabFallT = 0.0;
	LabImpactV = 0.0;
	LabSettleT = 0.0;
	LabMaxJitter = 0.0;
	LabSummary = TEXT("Lab running: 10kg @ 5m falling...");
	// Per-tick sampling (same guard pattern as the self-test: at most one
	// execution per world tick, so headless slow ticks cannot skew time).
	LastStepTime = -1.f;
	W->GetTimerManager().SetTimer(LabTimer, this,
		&UPhysicsValidationSubsystem::StepLab, 0.005f, true);
	UE_LOG(LogAfterfall, Log, TEXT("PHYLAB start: 10kg from 500cm (expect E=490.5J v=9.90m/s p=99.0)"));
}

void UPhysicsValidationSubsystem::OnLabHit(UPrimitiveComponent* HitComp, AActor* Other,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bLabLogging || !bLabRunning)
	{
		return;
	}
	(void)HitComp; (void)OtherComp; (void)Hit;
	UE_LOG(LogAfterfall, Log, TEXT("PHYLAB hit other=%s impulse=(%.1f,%.1f,%.1f)cm/s-g? |N|=%.1f"),
		Other ? *Other->GetName() : TEXT("none"),
		NormalImpulse.X, NormalImpulse.Y, NormalImpulse.Z, NormalImpulse.Size());
}

void UPhysicsValidationSubsystem::StepLab()
{
	UWorld* W = GetWorld();
	if (!W || !bLabRunning || !LabBody || !LabMesh)
	{
		return;
	}
	const float NowT = W->TimeSeconds;
	if (NowT == LastStepTime)
	{
		return;
	}
	const float Dt = W->DeltaTimeSeconds;
	LastStepTime = NowT;
	constexpr float BodyHalf = 50.f;
	if (LabPhase == 0)
	{
		// Falling: sample until the body reaches the floor contact band.
		// Only downward samples count: the tick that enters the band already
		// carries the contact solver's post-bounce (upward) velocity, which
		// must not overwrite the last pre-impact reading.
		LabFallT += Dt;
		const double Z = LabBody->GetActorLocation().Z;
		const double Vz = LabMesh->GetPhysicsLinearVelocity().Z;
		if (Vz < 0.0)
		{
		LabImpactV = -Vz; // last pre-impact downward speed (cm/s, positive)
		}
		if (Z <= BodyHalf + 2.0 || LabFallT > 10.0)
		{
		// Analytical refs (g=9.81 SI; engine gravity 980cm/s^2 ~= 9.80).
		// v=sqrt(2*9.81*5)=9.905m/s=990.5cm/s; t=sqrt(2*5/9.81)=1.010s.
		// Tolerance: +/-5% on v (30Hz tick granularity: up to ~33cm of fall
		// per tick at impact speed), +/-2% on t. E/p below are MEASURED
		// from the sampled impact speed (E=1/2 m v^2), not the reference.
		const double ExpectV = 990.5;
		const double ExpectT = 1.010;
		const double ErrV = (LabImpactV - ExpectV) / ExpectV * 100.0;
		const double ErrT = (LabFallT - ExpectT) / ExpectT * 100.0;
		const double Vms = LabImpactV / 100.0; // measured m/s
		const double MeasE = 0.5 * 10.0 * Vms * Vms; // measured J
		const double Momentum = 10.0 * Vms; // measured kg*m/s
		UE_LOG(LogAfterfall, Log,
			TEXT("PHYLAB impact v=%.1fcm/s (expect %.1f, err %+.2f%%, tol 5%%) t=%.3fs (expect %.3f, err %+.2f%%, tol 2%%) Emeas=%.1fJ (ref 490.5) p=%.2fkgm/s (ref 99.05)"),
			LabImpactV, ExpectV, ErrV, LabFallT, ExpectT, ErrT, MeasE, Momentum);
		LabPhase = 1;
		LabSettleT = 0.0;
		}
	}
	else if (LabPhase == 1)
	{
		// Settling: 2s window, track residual jitter + rest height.
		LabSettleT += Dt;
		const double Speed = LabMesh->GetPhysicsLinearVelocity().Size();
		LabMaxJitter = FMath::Max(LabMaxJitter, Speed);
		LabRestZ = LabBody->GetActorLocation().Z;
		if (LabSettleT >= 2.0)
		{
		LabSummary = FString::Printf(TEXT("10kg/5m: v=%.0fcm/s t=%.2fs rest=%.1fcm jitter=%.1f"),
				LabImpactV, LabFallT, LabRestZ, LabMaxJitter);
		UE_LOG(LogAfterfall, Log, TEXT("PHYLAB settle rest=%.1fcm (expect 50) residual=%.2fcm/s maxjitter=%.1fcm/s %s"),
				LabRestZ, Speed, LabMaxJitter, *LabSummary);
		FinishLab(true);
		}
	}
}

void UPhysicsValidationSubsystem::FinishLab(bool bOk)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(LabTimer);
	}
	bLabRunning = false;
	if (!bOk && LabSummary.IsEmpty())
	{
		LabSummary = TEXT("Lab FAIL (see log).");
	}
	UE_LOG(LogAfterfall, Log, TEXT("PHYLAB %s"), bOk ? TEXT("COMPLETE") : TEXT("FAILED"));
}

static FAutoConsoleCommandWithWorld PhysicsLabCmd(
	TEXT("afPhysicsLab"),
	TEXT("AFTERFALL lab: 10kg body dropped 5m with analytical comparison (no exit)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (UPhysicsValidationSubsystem* S = W ? W->GetSubsystem<UPhysicsValidationSubsystem>() : nullptr)
		{
			S->RunPhysicsLab();
		}
	}));

static FAutoConsoleCommandWithWorld PhysicsLogCmd(
	TEXT("afPhysicsLog"),
	TEXT("AFTERFALL lab: toggle dev-only impact logging"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (UPhysicsValidationSubsystem* S = W ? W->GetSubsystem<UPhysicsValidationSubsystem>() : nullptr)
		{
			S->ToggleLabLogging();
		}
	}));

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
