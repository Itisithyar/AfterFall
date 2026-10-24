// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Survival) — server-authoritative multi-item wallet.
// Phase A: ItemId->count map + weight cap. Legacy Wood int kept for
// ConstructionValidator migration; mirrors AF_R_Wood entry.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct FInventoryEntry
{
	GENERATED_BODY()
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Count = 0;
};

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// Legacy single-currency (Wood). Kept until validator switches to plank/nail costs.
	UPROPERTY(ReplicatedUsing = OnRep_Wood, VisibleAnywhere, Category = "Afterfall|Inventory")
	int32 Wood = 10;

	// Generic wallet. Replicated. Server is the only writer.
	UPROPERTY(ReplicatedUsing = OnRep_Items, VisibleAnywhere, Category = "Afterfall|Inventory")
	TArray<FInventoryEntry> Items;

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Inventory")
	float MaxWeightKg = 30.f;

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	int32 GetWood() const { return Wood; }

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	int32 GetCount(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	float GetWeightKg() const;

	static float WeightForItem(FName ItemId);

	// Loot/salvage grant. Validates 0 < Amount <= 100. Kept for Wood migration.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddResource(int32 Amount);

	// Generic grant. Server validates id non-empty, 0 < count <= 100, weight cap.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddItem(FName ItemId, int32 Count);

	// Legacy consume (Wood). Single authority path for placement/repair/upgrade.
	bool ServerTryConsume(int32 Cost);

	// Generic consume. Returns true and debits when server holds enough.
	bool ServerTryConsumeItem(FName ItemId, int32 Count);

protected:
	UFUNCTION()
	void OnRep_Wood();
	UFUNCTION()
	void OnRep_Items();

	int32* FindEntry(FName ItemId);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
