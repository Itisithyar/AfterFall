// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Interaction) — one held object per pawn. Pickup/drop/
// throw are server-validated transactions: state, range, speed, and holder
// are all re-checked; failures leave state untouched.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CarryComponent.generated.h"

class APhysicalItem;
class UAfterfallMovementComponent;

/**
 * Single authoritative held slot. The carried actor follows the pawn while
 * held; release commits exactly one world transition (drop or throw).
 */
UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarryComponent();

	UPROPERTY(ReplicatedUsing = OnRep_Carried, VisibleAnywhere, Category = "Afterfall|Carry")
	APhysicalItem* CarriedItem = nullptr;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Carry")
	bool bIsCarrying = false;

	/** Max pickup reach in cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Carry")
	float PickupRangeCm = 300.f;

	/** Configured throw speed (cm/s). Clamped server-side. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Carry")
	float ThrowSpeedCmS = 900.f;

	/** Player-velocity inheritance factor (spec default 1.0). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Carry")
	float ThrowInheritFactor = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Carry")
	float MaxThrowSpeedCmS = 1200.f;

	/** Server: take a free nearby item into the held slot. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Carry")
	void ServerPickup(APhysicalItem* Item);

	/** Server: place the held item at the feet with zero velocity. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Carry")
	void ServerDrop();

	/** Server: release with v_player + dir * speed. Direction re-normalized. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Carry")
	void ServerThrow(const FVector& Direction);

	/** Current carry mass (0 when empty-handed). */
	float GetCarriedMassKg() const;

	/** Server: clear hands after an external atomic store (mat deposit). */
	void ServerClearAfterStore();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Carried();

	/** Minimum interval between releases (spam/dupe guard). */
	float LastReleaseTime = -10.f;

	UAfterfallMovementComponent* MoveComp();
	void SyncCarryFlag();
};
