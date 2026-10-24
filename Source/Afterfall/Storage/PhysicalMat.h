// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Storage) — the SEPARATE physical inventory: a deployed
// world mat with its own slots. Transfers with the wallet/hands are atomic:
// validate everything first, then commit once. Failed transfers change nothing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Persistence/RegionStateSave.h"
#include "PhysicalMat.generated.h"

class UStaticMeshComponent;
class UCarryComponent;

/** One mat slot. Stacks by ItemId; InstanceId binds unique singletons. */
USTRUCT(BlueprintType)
struct FMatSlot
{
	GENERATED_BODY()
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() FGuid InstanceId;
};

UCLASS()
class AFTERFALL_API APhysicalStorageMat : public AActor
{
	GENERATED_BODY()

public:
	APhysicalStorageMat();

	/** Persistent container identity (matches FContainerDelta.ContainerGuid). */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Mat")
	FGuid ContainerGuid;

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Mat")
	int32 MaxSlots = 12;

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Mat")
	int32 MaxStack = 99;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Mat")
	TArray<FMatSlot> Slots;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Mat")
	bool bDeployed = true;

	/**
	 * Packed (put away) vs spread on the ground. Packing hides the mesh
	 * and disables collision but keeps Slots/identity; deposit/withdraw
	 * reject while packed. Driven by the mat interaction component.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Packed, VisibleAnywhere, Category = "Afterfall|Mat")
	bool bPacked = false;

	/** Server: pack (hide) or spread (show) the mat. Safe any time. */
	void SetPacked(bool bInPacked);

	/**
	 * Server: move the caller's HELD item onto the mat. Atomic: every check
	 * passes before anything mutates; failure leaves hands + mat untouched.
	 * Returns deposited count (0 = rejected).
	 */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Mat")
	int32 ServerDeposit(AController* Caller);

	/**
	 * Server: move Count of Slots[SlotIndex] back into the world in front of
	 * the caller (spawned with the same ItemId). Atomic; 0 = rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Mat")
	int32 ServerWithdraw(AController* Caller, int32 SlotIndex, int32 Count);

	/** Total units of ItemId currently on the mat. */
	int32 GetMatCount(FName ItemId) const;

	/** Export for delta saves (wired to RegionStateSave by persistence pass). */
	FContainerDelta ExportDelta() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Mat")
	UStaticMeshComponent* Mesh = nullptr;

	UFUNCTION()
	void OnRep_Packed();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UCarryComponent* CarryOf(AController* Caller) const;
	bool InReach(AController* Caller) const;
};
