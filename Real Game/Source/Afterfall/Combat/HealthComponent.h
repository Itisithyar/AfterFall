// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Survival) — server-authoritative health. Client NEVER
// sets HP; only server applies damage/heal through validated entry points.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthDepleted);

/**
 * Minimal replicated health pool. Death is latched (bIsDead) and cleared
 * only by ServerRevive; the owner (character) drives corpse + respawn.
 */
UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Health")
	float MaxHP = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_HP, VisibleAnywhere, Category = "Afterfall|Health")
	float HP = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Health")
	bool bIsDead = false;

	/** Fired on server when HP first reaches 0. Owner handles corpse/respawn. */
	UPROPERTY(BlueprintAssignable, Category = "Afterfall|Health")
	FOnHealthDepleted OnHealthDepleted;

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Health")
	float GetHPRatio() const { return MaxHP > 0.f ? FMath::Clamp(HP / MaxHP, 0.f, 1.f) : 0.f; }

	/** Server-only damage. Validates range; ignores when dead or not authority. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Health")
	void ServerTakeDamage(float Amount);

	/** Server-only full restore + clear death latch. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Health")
	void ServerRevive();

	/** Server-only heal (bandage hook). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Health")
	void ServerHeal(float Amount);

	static float ClampDamage(float Amount) { return FMath::Clamp(Amount, 0.f, 200.f); }

protected:
	UFUNCTION()
	void OnRep_HP();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
