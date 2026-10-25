// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Simulation - Event-driven, proximity-aware physics scheduling.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SimulationScheduler.generated.h"

UENUM(BlueprintType)
enum class ESimulationState : uint8
{
	Active,
	Sleeping,
	Dormant
};

USTRUCT(BlueprintType)
struct FSimulationEntity
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Actor = nullptr;
	UPROPERTY() float LastActiveTime = 0.0f;
	UPROPERTY() bool bIsMoving = false;
	UPROPERTY() ESimulationState State = ESimulationState::Active;
	UPROPERTY() FGuid EntityId;
};

UCLASS()
class AFTERFALL_API USimulationScheduler : public UObject
{
	GENERATED_BODY()

public:
	static USimulationScheduler* Get(UWorld* World);

	void Initialize(UWorld* InWorld);
	void Tick(float DeltaTime);

	// Proximity management
	void UpdateProximity(float ActiveRadius = 3000.0f, float Hysteresis = 200.0f);
	void WakeEntity(AActor* Actor);
	void SleepEntity(AActor* Actor);

	// Event-driven wake
	void OnEvent(AActor* Source, const FName& EventType, const FVector& Location, float Radius);

	// Registration
	void RegisterEntity(AActor* Actor);
	void UnregisterEntity(AActor* Actor);

	// Stats
	int32 GetActiveCount() const { return ActiveEntities.Num(); }
	int32 GetSleepingCount() const { return SleepingEntities.Num(); }

protected:
	UPROPERTY() TObjectPtr<UWorld> World;
	TArray<TObjectPtr<AActor>> RegisteredEntities;
	TArray<TObjectPtr<AActor>> ActiveEntities;
	TArray<TObjectPtr<AActor>> SleepingEntities;
	float AccumulatedTime = 0.0f;
};
