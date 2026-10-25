// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Movement) — custom momentum model on top of engine
// collision. Sprint is an explicit 5s simulation-time state machine;
// carrying heavy caps speed to 0.894 m/s (89.4 cm/s).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AfterfallMovementComponent.generated.h"

/** Explicit sprint lifecycle. Transitions are simulation-time driven. */
UENUM(BlueprintType)
enum class EAfterfallSprintState : uint8
{
	Ready      UMETA(DisplayName = "Ready"),
	Sprinting  UMETA(DisplayName = "Sprinting"),
	Recovering UMETA(DisplayName = "Recovering")
};

/** Tunable momentum model. Units: cm, cm/s, cm/s^2 (UE convention). */
USTRUCT(BlueprintType)
struct FAfterfallMovementTuning
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float NormalMaxSpeed = 500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float SprintMaxSpeed = 800.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float NormalAcceleration = 800.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float SprintAcceleration = 1000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float BrakingDeceleration = 1000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float GroundFriction = 8.f;
	/** Fraction of ground control while airborne. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move") float AirControl = 0.25f;
	/** Exact sprint budget in simulation seconds (spec: 5.0). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Sprint") float SprintDurationSec = 5.f;
	/** Lockout after a fully spent sprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Sprint") float SprintRecoverySec = 3.f;
	/** Provisional heavy-carry cap: 2 mph = 89.4 cm/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Carry") float CarryCapCmS = 89.4f;
	/** Carried mass at/above this is "heavy". */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Carry") float HeavyMassKg = 5.f;
	/** Physics/gameplay fixed-step quantum (spec: 120 Hz). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Sim") float FixedStepSec = 1.f / 120.f;
	/** Max fixed steps consumed per tick (spiral-of-death guard). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Sim") int32 MaxStepsPerTick = 8;
};

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UAfterfallMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UAfterfallMovementComponent();

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move")
	FAfterfallMovementTuning Tuning;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Sprint")
	EAfterfallSprintState SprintState = EAfterfallSprintState::Ready;

	/** Remaining sprint budget (sim seconds). Valid while Sprinting. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Sprint")
	float SprintTimeLeft = 0.f;

	/** Remaining lockout (sim seconds). Valid while Recovering. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Sprint")
	float RecoveryTimeLeft = 0.f;

	/** True while hauling a heavy object. Server-authoritative. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Carry")
	bool bCarryingHeavy = false;
	/** Carried mass (kg) used to derive load ratio. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|Load")
	float CarriedMassKg = 0.f;
	/** Reference mass (kg) for dimensionless load ratio. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Load")
	float ReferenceMassKg = 15.f;
	/** Load ratio lambda = CarriedMassKg / ReferenceMassKg. */
	float LoadRatio = 0.f;
	/** Server writer for carried mass. Wallet + held-slot masses are summed
	 *  (each item counted exactly once); the heavy-haul flag derives from the
	 *  HELD item only, so wallet weight can neither trigger nor clear it. */
	void SetCarriedMass(float InventoryMassKg, float HeldMassKg = 0.f);

	// Pure load-state helpers (unit-tested, no engine state).
	static float ComputeTotalMassKg(float InventoryMassKg, float HeldMassKg);
	static float ComputeLoadRatio(float TotalMassKg, float ReferenceMassKg);
	static bool ComputeHeavyHaul(float HeldMassKg, float HeavyThresholdKg);

	/** Telemetry getters. */
	float GetLoadRatio() const { return LoadRatio; }
	float GetEffectiveMaxSpeed() const { return EffectiveMaxSpeed; }
	float GetEffectiveAcceleration() const { return EffectiveAcceleration; }
	float GetEffectiveBraking() const { return EffectiveBraking; }

	/** Client intent entry point. Routes to server when not authoritative. */
	void SetSprinting(bool bWantSprint);

	/** Server writer for carry state (called by the carry component). */
	void SetCarryingHeavy(bool bHeavy);

	/** Accumulated simulation time (fixed-step quanta). */
	float GetSimTimeSec() const { return SimTimeSec; }

	/** Public read of the current speed cap (GetMaxSpeed is protected). */
	float GetCurrentMaxSpeed() const { return GetMaxSpeed(); }

	// Pure, unit-tested helpers (no engine state).
	static int32 ConsumeFixedSteps(double& Accumulator, double DeltaTime, double StepSec, int32 MaxSteps);
	static void AdvanceSprint(EAfterfallSprintState& State, float& TimeLeft, float& RecoveryLeft,
		bool bWantSprint, float SprintDuration, float RecoveryDuration, float SimDelta);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	float SimTimeSec = 0.f;
	double StepAccumulator = 0.0;
	float EffectiveMaxSpeed = 0.f;
	float EffectiveAcceleration = 0.f;
	float EffectiveBraking = 0.f;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetSprinting(bool bInWantSprint);

	/** Server-side intent latch (set locally or via RPC). */
	bool bWantSprint = false;
};
