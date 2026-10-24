// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (World) — server-authoritative harvest. Placeholders OK for slice.
// Deterministic Guid + DepletedUntil; visuals/progress handled in Blueprint.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceNode.generated.h"

UENUM(BlueprintType)
enum class EResourceKind : uint8
{
	Wood UMETA(DisplayName = "Wood"),
	Stone UMETA(DisplayName = "Stone"),
	Hemp UMETA(DisplayName = "Hemp")
};

UCLASS()
class AFTERFALL_API AResourceNode : public AActor
{
	GENERATED_BODY()

public:
	AResourceNode();

	UPROPERTY(EditAnywhere, Category = "Afterfall|Node")
	EResourceKind Kind = EResourceKind::Wood;

	// Slice yields (assumptions): tree 3-5 / rock 4-8 / hemp 2-4.
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Node")
	int32 YieldMin = 3;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Node")
	int32 YieldMax = 5;

	// Server Unix time when harvestable again. 0 = available.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Node")
	int64 DepletedUntilUnix = 0;

	// Server-only harvest. Validates range (<3m), alive, not depleted, tool bonus.
	// Returns granted amount (0 = rejected). Caller grants via Inventory::ServerAddItem.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Gather")
	int32 ServerHarvest(bool bHasTool);

	static int32 YieldFor(EResourceKind K, float T01);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
