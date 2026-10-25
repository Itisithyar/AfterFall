// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: FLINT (GLM session) — see BoxWallRuntimeSubsystem.h.
#include "BoxWallRuntimeSubsystem.h"
#include "Structural/BoxWallStructure.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UBoxWallRuntimeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Only gameplay contexts (PIE / game): never the editor's own world.
	if (InWorld.WorldType != EWorldType::PIE && InWorld.WorldType != EWorldType::Game)
	{
		return;
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("afBoxWall")))
	{
		UE_LOG(LogTemp, Log, TEXT("BoxWall flag not found"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("BoxWall flag found"));
	FTimerHandle H;
	InWorld.GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]() { SpawnGrayboxDemoBox(); }));
}

void UBoxWallRuntimeSubsystem::SpawnGrayboxDemoBox()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	// Spawn relative to the local player start so the wall is reachable in
	// every gameplay context (PIE / game), roughly 9 m in front of spawn.
	FVector SpawnLoc(-200.f, 700.f, 0.f);
	if (const APlayerStart* Start = Cast<APlayerStart>(
		UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass())))
	{
		SpawnLoc = Start->GetActorLocation() + FVector(900.f, 0.f, 5.f);
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABoxWallStructure* Wall = W->SpawnActor<ABoxWallStructure>(
		ABoxWallStructure::StaticClass(),
		FTransform(FRotator::ZeroRotator, SpawnLoc), P);
	if (!Wall)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxWall spawn failed"));
		return;
	}
	// Default profile: 4 cubes wide on every face; a 2x2 door on the front
	// face demonstrates configurable openings.
	FBoxBuildingProfile Profile;
	FBoxOpening Door;
	Door.FaceIndex = 0;
	Door.MinCube = FIntPoint(1, 0);
	Door.MaxCube = FIntPoint(2, 1);
	Profile.Openings.Add(Door);
	Wall->ServerInitialize(Profile);
	UE_LOG(LogTemp, Log,
		TEXT("BoxWall demo spawned at %s (4-wide profile, front door opening)"),
		*SpawnLoc.ToString());
}
