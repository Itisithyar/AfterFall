// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (World) — persistent physical item. Identity (ItemId)
// survives pickup/drop/throw/mat transfer; the holder/location relationship
// is server-authoritative. Implements IInteractable so E picks it up.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Inventory/ItemTypes.h"
#include "PhysicalItem.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EItemWorldState : uint8
{
	Free   UMETA(DisplayName = "Free in world"),
	Held   UMETA(DisplayName = "Held by pawn"),
	Stored UMETA(DisplayName = "Stored (mat/container)")
};

UCLASS()
class AFTERFALL_API APhysicalItem : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APhysicalItem();

	// IInteractable: E focuses and picks up a free nearby item.
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AController* Caller) const override;
	virtual void ServerInteract_Implementation(AController* Caller) override;

	/** Persistent identity. Assigned at spawn; never changes. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Item")
	FGuid ItemId;

	UPROPERTY(EditAnywhere, Replicated, Category = "Afterfall|Item")
	FName ItemType = TEXT("AF_R_Scrap");

	/**
	 * Full instance payload (identity, count, condition, lineage) carried
	 * across the world <-> inventory transition. An inventory drop writes its
	 * slot instance here; a pickup restores it, so state survives the round
	 * trip instead of degrading to a bare ItemType. Spawns without a payload
	 * (ItemId None) fall back to ItemType with count 1.
	 */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Item")
	FItemInstance InstancePayload;

	UPROPERTY(EditAnywhere, Category = "Afterfall|Item")
	float MassKg = 2.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Item")
	EItemWorldState WorldState = EItemWorldState::Free;

	/** Release velocity of the last throw (for reconciliation/logging). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Item")
	FVector LastReleaseVelocity = FVector::ZeroVector;

	// Server-only transitions (each validates current state first).
	void ServerPickupBy(APawn* Holder);
	void ServerReleaseToWorld(const FVector& ReleaseVelocity);
	void ServerStore();

	APawn* GetHolder() const { return Holder.Get(); }
	bool IsFree() const { return WorldState == EItemWorldState::Free; }

	/** Throw release model: v_player + dir * speed (inheritance configurable). */
	static FVector ComputeReleaseVelocity(const FVector& PlayerVelocity, const FVector& Direction,
		float ThrowSpeed, float InheritFactor);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Item")
	UStaticMeshComponent* Mesh = nullptr;

	UPROPERTY(Replicated)
	TWeakObjectPtr<APawn> Holder;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
