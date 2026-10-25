// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (World) — server-authoritative harvest. Visible slice mesh
// (engine basic shape per kind) so no art asset is required. Implements
// IInteractable: prompt + range/depletion checks + single validated grant.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "ResourceNode.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EResourceKind : uint8
{
	Wood UMETA(DisplayName = "Wood"),
	Stone UMETA(DisplayName = "Stone"),
	Hemp UMETA(DisplayName = "Hemp")
};

UCLASS()
class AFTERFALL_API AResourceNode : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AResourceNode();

	// IInteractable (prompt + pre-check client-side, execution server-side).
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AController* Caller) const override;
	virtual void ServerInteract_Implementation(AController* Caller) override;

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

	// Last granted yield (0 = rejected). Set server-side by ServerHarvest.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Gather")
	int32 LastHarvestYield = 0;

	// Slice respawn delay after a successful harvest.
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Node")
	float RespawnDelaySec = 60.f;

	// Server-only harvest. Validates range (<3m), not depleted, tool bonus.
	// Result in LastHarvestYield; caller grants via Inventory::ServerAddItem.
	// (Server RPCs cannot return values — UBT requirement.)
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Gather")
	void ServerHarvest(bool bHasTool);

	static int32 YieldFor(EResourceKind K, float T01);

	bool IsDepleted() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Node")
	UStaticMeshComponent* Mesh = nullptr;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Deterministic slice yield (midpoint) so tests and game agree. */
	int32 SliceYield(bool bHasTool) const;
};
