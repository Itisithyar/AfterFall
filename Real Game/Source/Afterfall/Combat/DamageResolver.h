// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: AI - Damage resolution pipeline (pure, testable).
#pragma once

#include "CoreMinimal.h"
#include "Combat/BodyDamageTypes.h"
#include "UObject/NoExportTypes.h"
#include "DamageResolver.generated.h"

USTRUCT(BlueprintType)
struct FDamageResult
{
	GENERATED_BODY()

	UPROPERTY() bool bHitResolved = false;
	UPROPERTY() bool bIsConfirmedHit = false;
	UPROPERTY() EBodyPart ResolvedPart = EBodyPart::Torso;
	UPROPERTY() float DamageDealt = 0.0f;
	UPROPERTY() float DamageAfterMitigation = 0.0f;
	UPROPERTY() bool bKilled = false;
	UPROPERTY() bool bLethalHeadShot = false;
};

UCLASS()
class AFTERFALL_API UDamageResolver : public UObject
{
	GENERATED_BODY()

public:
	// Resolve a hit: identifies location/part, applies weapon and armor rules
	static FDamageResult ResolveHit(
		EBodyPart HitPart,
		const FWeaponDamageConfig& WeaponConfig,
		const FArmorMitigationConfig& ArmorConfig,
		const TArray<FBodyPartConfig>& BodyParts,
		float ZombieMaxHealth,
		EDamageLethalCondition LethalCondition,
		float HeadLethalThreshold,
		float CurrentTotalHP,
		const TArray<float>& CurrentPerPartDamage);

	// Calculate damage for a specific part with multipliers
	static float CalculatePartDamage(EBodyPart Part, const FWeaponDamageConfig& WeaponConfig, const TArray<FBodyPartConfig>& BodyParts);

	// Check if hit is a confirmed hit (not a miss/rejected)
	static bool IsConfirmedHit(bool bHitRegistered, bool bInRange, bool bValidTarget, bool bAlive);

	// Determine if lethal based on rules (fills bLethalHeadShot on Result)
	static bool IsLethal(FDamageResult& Result, float NewTotalHP, float NewHeadDamage, float HeadLethalThreshold, EDamageLethalCondition LethalCondition);

	// Get body part index
	static int32 GetPartIndex(EBodyPart Part);
};
