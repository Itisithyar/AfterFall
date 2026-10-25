#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/AfterfallInventoryComponent.h"
#include "WorldCacheActor.generated.h"

UCLASS()
class AFTERFALL_API AWorldCacheActor : public AActor
{
	GENERATED_BODY()
public:
	AWorldCacheActor() { Inventory = CreateDefaultSubobject<UAfterfallInventoryComponent>(TEXT("Inv")); Inventory->bIsWorldCache = true; }
	UPROPERTY(VisibleAnywhere) UAfterfallInventoryComponent* Inventory = nullptr;
};