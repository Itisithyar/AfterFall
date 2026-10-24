// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: AI — slice zombie. Ghost-capable: no per-frame tick when dormant.
// Full behavior only when relevant; state via EntityLifecycleService.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZombiePrototype.generated.h"

UCLASS()
class AFTERFALL_API AZombiePrototype : public ACharacter
{
	GENERATED_BODY()

public:
	AZombiePrototype();

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Zombie")
	float HP = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Zombie")
	FGuid StableId;

	// 30m persistence floor; 100m active ceiling (see lifecycle service).
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie")
	float PersistRangeM = 30.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie")
	float ActiveRangeM = 100.f;

	// Server-only: approach player, damage barricade/actor, take damage, die.
	// Returns true while alive. Death tombstones + loot via LootService corpse table.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Zombie")
	bool ServerTakeDamage(float Amount);

	// Modular tick gate: Blueprint/AI calls this at reduced rate when dormant.
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Zombie")
	bool ShouldTickFull(float NearestPlayerDistM, bool bHasPursuer) const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
