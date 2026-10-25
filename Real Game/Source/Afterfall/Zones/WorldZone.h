// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Zones - POI-based world zone management
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldZone.generated.h"

UCLASS()
class AFTERFALL_API AWorldZone : public AActor
{
	GENERATED_BODY()

public:
	AWorldZone();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FString ZoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	float ZoneRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	bool bIsPOI = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	TArray<FName> LootProfiles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	bool bIsLoaded = false;

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void LoadZone();
	
	UFUNCTION(BlueprintCallable, Category = "Zone")
	void UnloadZone();
};
