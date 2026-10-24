// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: AI - Body-part damage configuration (data-driven, testable).
#pragma once

#include "CoreMinimal.h"
#include "BodyDamageTypes.generated.h"

// Canonical body-part set. Owned by the damage domain so both the zombie
// actor and the pure resolution pipeline share one definition.
UENUM(BlueprintType)
enum class EBodyPart : uint8
{
	Head   UMETA(DisplayName = "Head"),
	Torso  UMETA(DisplayName = "Torso"),
	ArmL   UMETA(DisplayName = "Left arm"),
	ArmR   UMETA(DisplayName = "Right arm"),
	LegL   UMETA(DisplayName = "Left leg"),
	LegR   UMETA(DisplayName = "Right leg")
};

UENUM(BlueprintType)
enum class EDamageLethalCondition : uint8
{
	// Kill when total HP depleted (torso/aggregate model)
	TotalHP UMETA(DisplayName = "Total HP"),
	// Kill if head damage reaches lethal threshold
	HeadLethal UMETA(DisplayName = "Head Lethal"),
	// Either condition can kill
	Any UMETA(DisplayName = "Any")
};

USTRUCT(BlueprintType)
struct FBodyPartConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	FName PartName;

	// Base health for this body part (if using per-part HP model)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	// Damage multiplier applied to incoming damage for this part
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	// If true, damage to this part can be lethal under configured rules
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bCanBeLethal = false;

	// Threshold for lethal damage if using per-part lethal threshold
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float LethalThreshold = 0.0f;
};

USTRUCT(BlueprintType)
struct FWeaponDamageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	FName WeaponId = FName("StandardSlice");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	FName AmmoType = FName("Standard");

	// Base damage before part multipliers
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 17.0f;

	// If true, this is the designated weapon for the six-shot baseline
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bIsDesignatedSixShotWeapon = true;
};

USTRUCT(BlueprintType)
struct FArmorMitigationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	FName ArmorType = FName("Unarmored");

	// Damage reduction as percentage (0.0 = no reduction, 1.0 = full reduction)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageReduction = 0.0f;

	// Per-part mitigation overrides
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TMap<FName, float> PerPartReduction;
};

UCLASS(BlueprintType)
class AFTERFALL_API UBodyDamageDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Designated weapon configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FWeaponDamageConfig WeaponConfig;

	// Body part configurations
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BodyParts")
	TArray<FBodyPartConfig> BodyParts;

	// Armor configurations
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor")
	TArray<FArmorMitigationConfig> ArmorConfigs;

	// Default zombie health (aggregate)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie", meta = (ClampMin = "0.0"))
	float ZombieMaxHealth = 100.0f;

	// Lethal condition
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	EDamageLethalCondition LethalCondition = EDamageLethalCondition::TotalHP;

	// Head lethal threshold if using head-lethal rule
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie", meta = (ClampMin = "0.0"))
	float HeadLethalThreshold = 100.0f;

	// Six-shot baseline target (for validation)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance")
	int32 SixShotBaselineTarget = 6;
};
