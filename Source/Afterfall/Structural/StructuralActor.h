// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Structural team — single actor class for generated + player-built parts.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructuralTypes.h"
#include "StructuralActor.generated.h"

UCLASS()
class AFTERFALL_API AStructuralActor : public AActor
{
	GENERATED_BODY()

public:
	AStructuralActor();

	// Canonical spec — set ONLY by server (construction validation or proc-gen).
	UPROPERTY(ReplicatedUsing = OnRep_Spec, EditDefaultsOnly, Category = "Structural")
	FStructuralSpec Spec;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural")
	float Durability = 100.f;

	// Owning player / claimed building ID. Server-authoritative.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural|Ownership")
	int32 OwnerPlayerId = -1;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Structural|Ownership")
	bool bIsClaimed = false;

	// Server-only API. Called by GameMode/ConstructionSubsystem after validation.
	// Never call from client.
	void ServerInitialize(const FStructuralSpec& InSpec, int32 InOwnerId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerApplyDamage(float DamageAmount);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRepair();

protected:
	UFUNCTION()
	void OnRep_Spec();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
