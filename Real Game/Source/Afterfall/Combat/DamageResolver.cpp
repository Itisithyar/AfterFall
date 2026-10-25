// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "DamageResolver.h"
#include "Afterfall.h"

int32 UDamageResolver::GetPartIndex(EBodyPart Part)
{
	return static_cast<int32>(Part);
}

float UDamageResolver::CalculatePartDamage(EBodyPart Part, const FWeaponDamageConfig& WeaponConfig, const TArray<FBodyPartConfig>& BodyParts)
{
	float BaseDamage = WeaponConfig.BaseDamage;
	float Multiplier = 1.0f;

	// Look up part config for multiplier
	for (const auto& PartConfig : BodyParts)
	{
		// Map part enum to config - simple approach
		if ((Part == EBodyPart::Head && PartConfig.PartName == FName("Head")) ||
			(Part == EBodyPart::Torso && PartConfig.PartName == FName("Torso")) ||
			(Part == EBodyPart::ArmL && PartConfig.PartName == FName("LeftArm")) ||
			(Part == EBodyPart::ArmR && PartConfig.PartName == FName("RightArm")) ||
			(Part == EBodyPart::LegL && PartConfig.PartName == FName("LeftLeg")) ||
			(Part == EBodyPart::LegR && PartConfig.PartName == FName("RightLeg")))
		{
			Multiplier = PartConfig.DamageMultiplier;
			break;
		}
	}

	// Fallback to legacy values if no config match
	if (Multiplier == 1.0f && BodyParts.Num() == 0)
	{
		switch (Part)
		{
		case EBodyPart::Head: return 34.0f;
		case EBodyPart::Torso: return 17.0f;
		default: return 12.0f;
		}
	}

	return BaseDamage * Multiplier;
}

bool UDamageResolver::IsConfirmedHit(bool bHitRegistered, bool bInRange, bool bValidTarget, bool bAlive)
{
	if (!bHitRegistered) return false;
	if (!bInRange) return false;
	if (!bValidTarget) return false;
	if (!bAlive) return false;
	return true;
}

FDamageResult UDamageResolver::ResolveHit(
	EBodyPart HitPart,
	const FWeaponDamageConfig& WeaponConfig,
	const FArmorMitigationConfig& ArmorConfig,
	const TArray<FBodyPartConfig>& BodyParts,
	float ZombieMaxHealth,
	EDamageLethalCondition LethalCondition,
	float HeadLethalThreshold,
	float CurrentTotalHP,
	const TArray<float>& CurrentPerPartDamage)
{
	FDamageResult Result;
	Result.ResolvedPart = HitPart;
	Result.bHitResolved = true;
	Result.bIsConfirmedHit = true;

	float Damage = CalculatePartDamage(HitPart, WeaponConfig, BodyParts);
	Result.DamageDealt = Damage;

	// Apply armor mitigation
	float Mitigation = ArmorConfig.DamageReduction;
	FName PartName;
	switch (HitPart)
	{
	case EBodyPart::Head: PartName = FName("Head"); break;
	case EBodyPart::Torso: PartName = FName("Torso"); break;
	case EBodyPart::ArmL: PartName = FName("LeftArm"); break;
	case EBodyPart::ArmR: PartName = FName("RightArm"); break;
	case EBodyPart::LegL: PartName = FName("LeftLeg"); break;
	case EBodyPart::LegR: PartName = FName("RightLeg"); break;
	default: PartName = FName("Torso"); break;
	}

	if (ArmorConfig.PerPartReduction.Contains(PartName))
	{
		Mitigation = FMath::Max(Mitigation, ArmorConfig.PerPartReduction[PartName]);
	}

	Result.DamageAfterMitigation = Damage * (1.0f - FMath::Clamp(Mitigation, 0.0f, 1.0f));
	return Result;
}

bool UDamageResolver::IsLethal(FDamageResult& Result, float NewTotalHP, float NewHeadDamage, float HeadLethalThreshold, EDamageLethalCondition LethalCondition)
{
	if (Result.ResolvedPart == EBodyPart::Head)
	{
		Result.bLethalHeadShot = (NewHeadDamage >= HeadLethalThreshold);
	}

	if (LethalCondition == EDamageLethalCondition::HeadLethal)
	{
		if (Result.ResolvedPart == EBodyPart::Head && NewHeadDamage >= HeadLethalThreshold)
		{
			return true;
		}
		return false;
	}
	else if (LethalCondition == EDamageLethalCondition::Any)
	{
		if (Result.ResolvedPart == EBodyPart::Head && NewHeadDamage >= HeadLethalThreshold)
		{
			return true;
		}
		if (NewTotalHP <= 0.0f)
		{
			return true;
		}
		return false;
	}
	else // TotalHP
	{
		return NewTotalHP <= 0.0f;
	}
}
