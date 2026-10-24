#include "PhysicsTestSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
static UStaticMesh* CubeMesh = nullptr;
}

void UPhysicsTestSubsystem::RunPhysicsTest()
{
	UWorld* W = GetWorld();
	if (!W) { UE_LOG(LogAfterfall, Warning, TEXT("PHYSICSTEST no world")); return; }
	UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST started"));
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FVector(0,0,500), FRotator::ZeroRotator);
	if (!A) return;
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(A);
	A->SetRootComponent(M);
	M->SetMobility(EComponentMobility::Movable);
	M->RegisterComponent();
	if (!CubeMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
	}
	if (!CubeMesh || !M) { UE_LOG(LogAfterfall, Warning, TEXT("PHYSICSTEST missing mesh")); FPlatformMisc::RequestExit(1); return; }
	M->SetStaticMesh(CubeMesh);
	M->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	M->SetSimulatePhysics(true);
	M->SetMassOverrideInKg(NAME_None, 10.f);
	M->WakeAllRigidBodies();
	UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST mesh=%s simulate=%d"), CubeMesh ? *CubeMesh->GetName() : TEXT("null"), M->IsSimulatingPhysics());
	FVector Start = A->GetActorLocation();
	FTimerHandle H;
	W->GetTimerManager().SetTimer(H, [W, A, Start]()
	{
		FVector End = A->GetActorLocation();
		float Dz = Start.Z - End.Z;
		UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST freefall dz=%.2f cm (ideal 490.5)"), Dz);
		// impulse test
		UStaticMeshComponent* M = Cast<UStaticMeshComponent>(A->GetRootComponent());
		if (M) { M->AddImpulse(FVector(100.f,0,0), NAME_None, true); }
		FTimerHandle H2;
		W->GetTimerManager().SetTimer(H2, [W, A]()
		{
			UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST impulse done"));
			FPlatformMisc::RequestExit(0);
		}, 0.5f, false);
	}, 1.0f, false);
}

static void AFPhysPostWorldInit(UWorld* W, const UWorld::InitializationValues IVS)
{
	UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST hook fired, world=%p"), W);
	if (!W) return;
	if (!FParse::Param(FCommandLine::Get(), TEXT("afPhysicsTest"))) { UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST flag not found")); return; }
	UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST flag found"));
	if (UPhysicsTestSubsystem* S = W->GetSubsystem<UPhysicsTestSubsystem>())
	{
		UE_LOG(LogAfterfall, Log, TEXT("PHYSICSTEST subsystem found, scheduling"));
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, S, &UPhysicsTestSubsystem::RunPhysicsTest, 1.0f, false);
	}
	else
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PHYSICSTEST subsystem null"));
	}
}

static struct FAFPhysHook
{
	FAFPhysHook()
	{
		FWorldDelegates::OnPostWorldInitialization.AddStatic(&AFPhysPostWorldInit);
	}
} GAFPhysHook;

static FAutoConsoleCommandWithWorld PhysCmd(
	TEXT("afPhysicsTest"),
	TEXT("physics test"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (UPhysicsTestSubsystem* S2 = W ? W->GetSubsystem<UPhysicsTestSubsystem>() : nullptr)
		{
			S2->RunPhysicsTest();
		}
	}));
