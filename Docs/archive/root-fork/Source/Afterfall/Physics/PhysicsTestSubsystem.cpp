#include "PhysicsTestSubsystem.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Afterfall.h"

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
	M->RegisterComponent();
	if (!CubeMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	M->SetStaticMesh(CubeMesh);
	M->SetSimulatePhysics(true);
	M->SetMassOverrideInKg(NAME_None, 1.f);
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
