// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: BEAM Free (Phase 1 medical) — server-authoritative per-body-part
// injury records + manual treatment sessions. NEW FILES ONLY: reads the
// public HealthComponent (ServerTakeDamage) and InventoryComponent
// (GetCount/ServerTryConsumeItem) APIs; modifies neither. HP restoration is
// recorded as PendingHPRestore for the Phase 2 HealthComponent heal entry
// point — never applied directly, never client-trusted.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BodyDamageTypes.h"
#include "InjuryStateComponent.generated.h"

/** Treatment session lifecycle (mirrors the mat open/close discipline). */
UENUM(BlueprintType)
enum class ETreatmentState : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	Treating   UMETA(DisplayName = "Treating (application in progress)"),
	Complete   UMETA(DisplayName = "Complete"),
	Interrupted UMETA(DisplayName = "Interrupted")
};

/** Authoritative record for one body part. Replicated as an array of 6. */
USTRUCT(BlueprintType)
struct FInjuryRecord
{
	GENERATED_BODY()

	/** Part index in EBodyPart order (Head=0 .. LegR=5). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	int32 PartIndex = 0;

	/** A wound is present (set by damage, cleared only by full treatment). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	bool bWounded = false;

	/** 0 = none, 1 = light, 2 = severe, 3 = critical. */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	int32 Severity = 0;

	/** Active bleeding (drains HP server-side while true). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	bool bBleeding = false;

	/** HP drained per second while bleeding. */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	float BleedRateHpS = 0.f;

	/** Pain flag (relieved by compatible items). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	bool bPain = false;

	/** Stabilized severe wound (enables recovery; set by compatible items). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Injury")
	bool bStabilized = false;
};

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UInjuryStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInjuryStateComponent();

	/** Six records in EBodyPart order. Replicated. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Injury")
	TArray<FInjuryRecord> PartInjuries;

	/** Current treatment session state. Replicated. */
	UPROPERTY(ReplicatedUsing = OnRep_Treatment, VisibleAnywhere, Category = "Afterfall|Medical")
	ETreatmentState TreatmentState = ETreatmentState::Idle;

	/** Part under treatment (valid while Treating). */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Medical")
	int32 TreatingPartIndex = -1;

	/** Item being applied (valid while Treating). */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Medical")
	FName TreatingItemId = NAME_None;

	/** 0..1 progress of the active application. Replicated. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Medical")
	float TreatmentProgress = 0.f;

	/**
	 * HP restore granted by the last completed treatment, awaiting the
	 * HealthComponent heal entry point (Phase 2 owner hook). Replicated so
	 * the pending grant survives replication; ConsumePendingHeal reads+clears.
	 */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Medical")
	float PendingHPRestore = 0.f;

	/** Server: record a wound from confirmed damage (Phase 2 hooks the
	 * damage pipeline to this; callable standalone for tests/cheats). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Injury")
	void ServerApplyWound(EBodyPart Part, int32 Severity);

	/** Server: begin a manual treatment application. Validates session idle,
	 * part wounded, capability matches injury, owner holds the item. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Medical")
	void ServerBeginTreatment(EBodyPart Part, FName ItemId);

	/** Server: abort the active session (interrupt, damage, move, death).
	 * Never consumes the item; progress is discarded. Safe any time. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Medical")
	void ServerInterruptTreatment();

	/** Server per-frame (or timer) driver: bleed drain + treatment progress.
	 * No world required beyond owner components; null-safe for tests. */
	void ServerAdvance(float DeltaSeconds);

	/** Reads + clears PendingHPRestore (Phase 2 heal path consumes this). */
	float ConsumePendingHeal();

	// Pure, unit-tested helpers (no engine state).
	static bool CanTransition(ETreatmentState From, ETreatmentState To);
	static int32 PartToIndex(EBodyPart Part);
	static float BleedTick(float RateHpS, float DeltaSeconds);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Treatment();

	/** Active session id (duplicate-request protection). */
	FGuid ActiveSessionId;

	float TreatmentElapsed = 0.f;
	float TreatmentDuration = 0.f;

	class UHealthComponent* HealthOfOwner() const;
	class UInventoryComponent* InventoryOfOwner() const;
};
