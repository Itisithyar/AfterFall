// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Character) — first-person pawn. Placement preview is
// client-side ONLY; ServerRequestPlacement is the single authority path.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Structural/StructuralTypes.h"
#include "AfterfallCharacter.generated.h"

class UInventoryComponent;
class AStructuralActor;

UCLASS()
class AFTERFALL_API AAfterfallCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAfterfallCharacter();

	// Server-authoritative wallet. Created as default subobject; replicated.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Inventory")
	UInventoryComponent* Inventory;

	// Client builds preview from Spec table; ghost material only, no spawn.
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Build")
	FPlacementRequest BuildPreviewRequest(EStructuralPart Part, FVector HitLocation, FRotator ViewRotation) const;

	// Single server-authoritative placement entry. Server validates grid,
	// support, cost, ownership — then spawns AStructuralActor.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Build")
	void ServerRequestPlacement(const FPlacementRequest& Request);

	// Repair target to full durability. Cost deducted server-side from Inventory.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Build")
	void ServerRequestRepair(AStructuralActor* Target);

	// Upgrade tier 0 -> 1 (2 Wood). Server validates + deducts.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Build")
	void ServerRequestUpgrade(AStructuralActor* Target);

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
};
