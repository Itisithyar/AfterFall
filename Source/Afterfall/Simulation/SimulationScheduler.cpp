// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "SimulationScheduler.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"

static TWeakObjectPtr<USimulationScheduler> GSimulationSchedulerInstance;

USimulationScheduler* USimulationScheduler::Get(UWorld* World)
{
	if (!GSimulationSchedulerInstance.IsValid())
	{
		GSimulationSchedulerInstance = NewObject<USimulationScheduler>(GetTransientPackage());
		if (GSimulationSchedulerInstance.IsValid())
		{
			GSimulationSchedulerInstance->Initialize(World);
		}
	}
	else if (World && GSimulationSchedulerInstance->World != World)
	{
		GSimulationSchedulerInstance->Initialize(World);
	}
	return GSimulationSchedulerInstance.Get();
}

void USimulationScheduler::Initialize(UWorld* InWorld)
{
	World = InWorld;
	RegisteredEntities.Empty();
	ActiveEntities.Empty();
	SleepingEntities.Empty();
}

void USimulationScheduler::Tick(float DeltaTime)
{
	AccumulatedTime += DeltaTime;
	UpdateProximity(3000.0f, 200.0f); // 30m = 3000cm, 2m hysteresis
}

void USimulationScheduler::UpdateProximity(float ActiveRadius, float Hysteresis)
{
	if (!World) return;
	
	APawn* PlayerPawn = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			PlayerPawn = PC->GetPawn();
			break;
		}
	}
	if (!PlayerPawn) return;
	
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	
	for (auto& Entity : RegisteredEntities)
	{
		AActor* Actor = Entity.Get();
		if (!Actor || Actor->IsPendingKillPending()) continue;
		
		float Dist = FVector::Dist(PlayerLocation, Actor->GetActorLocation());
		bool bShouldBeActive = (Dist <= ActiveRadius);
		
		// Check if moving - moving objects must stay active
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			if (Prim->IsSimulatingPhysics())
			{
				FVector Vel = Prim->GetPhysicsLinearVelocity();
				if (Vel.SizeSquared() > 1.0f) // Moving
				{
					bShouldBeActive = true;
				}
			}
		}
		
		// Check if in active list
		bool bIsActive = ActiveEntities.Contains(Actor);
		
		if (bShouldBeActive && !bIsActive)
		{
			WakeEntity(Actor);
		}
		else if (!bShouldBeActive && bIsActive)
		{
			// Use hysteresis - only sleep if outside radius + hysteresis
			if (Dist > ActiveRadius + Hysteresis)
			{
				SleepEntity(Actor);
			}
		}
	}
}

void USimulationScheduler::WakeEntity(AActor* Actor)
{
	if (!Actor) return;
	if (ActiveEntities.Contains(Actor)) return;
	
	ActiveEntities.Add(Actor);
	SleepingEntities.Remove(Actor);
	
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		if (Prim->IsSimulatingPhysics())
		{
			Prim->WakeAllRigidBodies(); // no-op when already awake
		}
	}
}

void USimulationScheduler::SleepEntity(AActor* Actor)
{
	if (!Actor) return;
	if (SleepingEntities.Contains(Actor)) return;
	
	// Don't sleep if moving
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		if (Prim->IsSimulatingPhysics())
		{
			FVector Vel = Prim->GetPhysicsLinearVelocity();
			if (Vel.SizeSquared() > 1.0f) return;
		}
	}
	
	SleepingEntities.Add(Actor);
	ActiveEntities.Remove(Actor);
	
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		if (Prim->IsSimulatingPhysics())
		{
			Prim->PutAllRigidBodiesToSleep();
		}
	}
}

void USimulationScheduler::OnEvent(AActor* Source, const FName& EventType, const FVector& Location, float Radius)
{
	if (!World) return;
	for (auto& Entity : RegisteredEntities)
	{
		AActor* Actor = Entity.Get();
		if (!Actor || Actor->IsPendingKillPending()) continue;
		
		float Dist = FVector::Dist(Location, Actor->GetActorLocation());
		if (Dist <= Radius)
		{
			WakeEntity(Actor);
		}
	}
}

void USimulationScheduler::RegisterEntity(AActor* Actor)
{
	if (!Actor) return;
	if (!RegisteredEntities.Contains(Actor))
	{
		RegisteredEntities.Add(Actor);
		ActiveEntities.Add(Actor);
	}
}

void USimulationScheduler::UnregisterEntity(AActor* Actor)
{
	if (!Actor) return;
	RegisteredEntities.Remove(Actor);
	ActiveEntities.Remove(Actor);
	SleepingEntities.Remove(Actor);
}
