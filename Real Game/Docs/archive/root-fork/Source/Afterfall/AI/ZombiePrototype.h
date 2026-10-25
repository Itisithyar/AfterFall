// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: AI â€” slice zombie. Visible humanoid placeholder built from engine
// basic shapes (head/torso/2 arms/2 legs) so no art asset is required; parts
// swap 1:1 for a skeletal mesh later without touching damage/persistence.
//
// Damage model (server-authoritative, standard slice weapon):
//   Head 34 (3 hits) / Torso 17 (6 hits = baseline kill) / Limbs 12.
// First confirmed damage assigns StableId (one record); later hits update it.
// Undamaged zombies carry no individual record.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Entities/EntityLifecycle.h"
#include "Combat/BodyDamageTypes.h"
#include "Combat/DamageResolver.h"
#include "ZombiePrototype.generated.h"

class UStaticMeshComponent;

// EBodyPart lives in Combat/BodyDamageTypes.h (shared with DamageResolver).
UENUM()
enum class EZombieState : uint8
{
	Wander,
	Chase,
	Attack,
	Dead
};

/** Server-side damage record. Created once on first confirmed hit. */
USTRUCT()
struct FZombieDamageRecord
{
	GENERATED_BODY()
	UPROPERTY() FGuid StableId;
	UPROPERTY() bool bHasRecord = false;
	/** Damage accumulated per EBodyPart index. Plain (server-side record). */
	float PerPart[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	UPROPERTY() float TotalDealt = 0.f;
	/** Per-part current health if using per-part health model. Plain member. */
	float PerPartHealth[6] = { 100.f, 100.f, 100.f, 100.f, 100.f, 100.f };
	/** Whether part is damaged/broken. Plain member (bool arrays need no reflection). */
	bool bPartDamaged[6] = { false, false, false, false, false, false };
	/** Status effects */
	UPROPERTY() TArray<FName> StatusEffects;
};

UCLASS()
class AFTERFALL_API AZombiePrototype : public ACharacter
{
	GENERATED_BODY()

public:
	AZombiePrototype();

	// Six visible body parts (engine basic shapes, placeholder grey).
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Zombie|Body")
	UStaticMeshComponent* PartHead = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Zombie|Body")
	UStaticMeshComponent* PartTorso = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Zombie|Body")
	UStaticMeshComponent* PartArmL = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Zombie|Body")
	UStaticMeshComponent* PartArmR = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Zombie|Body")
	UStaticMeshComponent* PartLegL = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Zombie|Body")
	UStaticMeshComponent* PartLegR = nullptr;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Zombie")
	float HP = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Zombie")
	FGuid StableId;

	// Damage configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Afterfall|Zombie|Damage")
	UBodyDamageDataAsset* DamageConfig = nullptr;

	// 30m persistence floor; 100m active ceiling (see lifecycle service).
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie")
	float PersistRangeM = 30.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie")
	float ActiveRangeM = 100.f;

	// Slice senses/rates (tuned for a slow prototype shambler).
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie|AI")
	float SenseRangeM = 25.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie|AI")
	float AttackRangeM = 2.2f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie|AI")
	float DisengageRangeM = 40.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie|AI")
	float MeleeDamage = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie|AI")
	float ChaseSpeed = 220.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Zombie|AI")
	float WanderSpeed = 60.f;

	/** Server-only: apply one confirmed part hit from an instigator in range. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Zombie")
	void ServerHitPart(EBodyPart Part, AController* Attacker);

	/** Apply damage with full pipeline - creates record on first confirmed hit */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Zombie")
	void ServerApplyDamage(EBodyPart Part, float Damage, AController* Attacker);

	// Server-only: approach player, damage barricade/actor, take damage, die.
	// Death tombstones + loot via LootService corpse table. Check IsAlive after.
	// (Server RPCs cannot return values â€” UBT requirement.)
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Zombie")
	void ServerTakeDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Zombie")
	bool IsAlive() const { return State != EZombieState::Dead && HP > 0.f; }

	// Modular tick gate: Blueprint/AI calls this at reduced rate when dormant.
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Zombie")
	bool ShouldTickFull(float NearestPlayerDistM, bool bHasPursuer) const;

	/** Pure damage table (testable): standard slice weapon vs body part. */
	static float DamageForPart(EBodyPart Part);

	/** Get damage using data-driven config */
	float GetDamageForPart(EBodyPart Part) const;

	/** Map a hit mesh component back to its body part via component tag. */
	static bool PartFromComponent(const UPrimitiveComponent* Comp, EBodyPart& Out);

	/** Export for persistence: HP + stable id follow the ghost schema. */
	FZombieGhost ToGhost() const;

	/** Server-side record (empty when undamaged â€” no record until first hit). */
	const FZombieDamageRecord& GetDamageRecord() const { return DamageRecord; }

	/** Check if zombie has any persistent damage record */
	bool HasDamageRecord() const { return DamageRecord.bHasRecord; }

	/** Get stable ID, creating if doesn't exist */
	FGuid GetOrCreateStableId();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	EZombieState State = EZombieState::Wander;
	FZombieDamageRecord DamageRecord;
	FDeathRecord DeathRecord;
	bool bDeathRecorded = false;
	FTimerHandle AITimer;
	FTimerHandle AttackCooldownTimer;
	bool bCanAttack = true;
	FVector WanderTarget = FVector::ZeroVector;
	float NextWanderTime = 0.f;
	FWeaponDamageConfig DefaultWeaponConfig;
	FArmorMitigationConfig DefaultArmorConfig;
	// Empty by default: resolver falls back to the legacy 34/17/12 table.
	TArray<FBodyPartConfig> DefaultBodyParts;

	UStaticMeshComponent* BuildPart(const TCHAR* Name, const TCHAR* MeshPath,
		FVector Location, FVector Scale, const TCHAR* Tag);

	void AITick();
	void ApplyDamage(float Amount, int32 PartIndex);
	void ApplyDamageWithPipeline(EBodyPart Part, float BaseDamage, AController* Attacker);
	void Die(const FString& Cause);
	void EnsureDamageRecord();
	void InitializeDefaultConfigs();
};
