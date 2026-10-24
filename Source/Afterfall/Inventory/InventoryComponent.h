// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Survival) — server-authoritative resource store.
// Client NEVER sets amounts directly; only ServerAddResource (loot/salvage,
// validated) and ServerConsumeResource (construction/repair, validated).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// Single-currency vertical-slice stub (Wood). Multi-resource in Phase 5.
	UPROPERTY(ReplicatedUsing = OnRep_Wood, VisibleAnywhere, Category = "Afterfall|Inventory")
	int32 Wood = 10;

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	int32 GetWood() const { return Wood; }

	// Loot/salvage grant. Server validates 0 < Amount <= 100 per grant.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddResource(int32 Amount);

	// Returns true and debits when server holds enough. Single authority path
	// for construction costs, repairs, upgrades.
	bool ServerTryConsume(int32 Cost);

protected:
	UFUNCTION()
	void OnRep_Wood();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
