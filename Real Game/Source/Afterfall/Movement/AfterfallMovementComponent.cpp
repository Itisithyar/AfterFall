// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

static TAutoConsoleVariable<int32> CVarLoadDebug(TEXT("af.LoadDebug"), 0, TEXT("Show load telemetry"), ECVF_Default);

UAfterfallMovementComponent::UAfterfallMovementComponent()
{
	// Baseline momentum feel; TickComponent re-applies per state each frame.
	MaxWalkSpeed = Tuning.NormalMaxSpeed;
	MaxAcceleration = Tuning.NormalAcceleration;
	BrakingDecelerationWalking = Tuning.BrakingDeceleration;
	GroundFriction = Tuning.GroundFriction;
	AirControl = Tuning.AirControl;
	bUseSeparateBrakingFriction = true;
	BrakingFrictionFactor = 1.f;
}

void UAfterfallMovementComponent::SetSprinting(bool bWant)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bWantSprint = bWant;
	}
	else
	{
		ServerSetSprinting(bWant);
	}
}

float UAfterfallMovementComponent::ComputeTotalMassKg(float InventoryMassKg, float HeldMassKg)
{
	// Each item contributes exactly once: the wallet and the held slot are
	// disjoint ownerships (a held item is never also inside the wallet).
	return FMath::Max(0.f, InventoryMassKg) + FMath::Max(0.f, HeldMassKg);
}

float UAfterfallMovementComponent::ComputeLoadRatio(float TotalMassKg, float ReferenceMassKg)
{
	// Dimensionless lambda = Total / Reference. Invalid (<=0) reference or
	// total -> 0: no load effect, never a division by zero or negative ratio.
	if (TotalMassKg <= 0.f || ReferenceMassKg <= 0.f)
	{
		return 0.f;
	}
	return TotalMassKg / ReferenceMassKg;
}

bool UAfterfallMovementComponent::ComputeHeavyHaul(float HeldMassKg, float HeavyThresholdKg)
{
	return HeavyThresholdKg > 0.f && HeldMassKg >= HeavyThresholdKg;
}

void UAfterfallMovementComponent::SetCarriedMass(float InventoryMassKg, float HeldMassKg)
{
	const float Total = ComputeTotalMassKg(InventoryMassKg, HeldMassKg);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CarriedMassKg = Total;
	}
	LoadRatio = ComputeLoadRatio(CarriedMassKg, ReferenceMassKg);
	bCarryingHeavy = ComputeHeavyHaul(HeldMassKg, Tuning.HeavyMassKg);
}

void UAfterfallMovementComponent::SetCarryingHeavy(bool bHeavy)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bCarryingHeavy = bHeavy;
	}
}

bool UAfterfallMovementComponent::ServerSetSprinting_Validate(bool /*bWant*/)
{
	return true;
}

void UAfterfallMovementComponent::ServerSetSprinting_Implementation(bool bInWantSprint)
{
	bWantSprint = bInWantSprint;
}

int32 UAfterfallMovementComponent::ConsumeFixedSteps(double& Accumulator, double DeltaTime,
	double StepSec, int32 MaxSteps)
{
	if (StepSec <= 0.0 || MaxSteps <= 0)
	{
		return 0;
	}
	Accumulator += DeltaTime;
	int32 Steps = 0;
	while (Accumulator >= StepSec && Steps < MaxSteps)
	{
		Accumulator -= StepSec;
		++Steps;
	}
	if (Steps == MaxSteps && Accumulator >= StepSec)
	{
		// Spiral guard: drop backlog rather than spiral. Sprint timing stays
		// conservative (never grants extra budget).
		Accumulator = 0.0;
	}
	return Steps;
}

void UAfterfallMovementComponent::AdvanceSprint(EAfterfallSprintState& State, float& TimeLeft,
	float& RecoveryLeft, bool bWantSprint, float SprintDuration, float RecoveryDuration, float SimDelta)
{
	switch (State)
	{
	case EAfterfallSprintState::Ready:
		if (bWantSprint)
		{
			State = EAfterfallSprintState::Sprinting;
			TimeLeft = SprintDuration;
		}
		break;
	case EAfterfallSprintState::Sprinting:
		if (!bWantSprint)
		{
			// Early release cancels with no lockout; only a SPENT sprint recovers.
			State = EAfterfallSprintState::Ready;
			TimeLeft = 0.f;
			break;
		}
		TimeLeft -= SimDelta;
		if (TimeLeft <= 0.f)
		{
			TimeLeft = 0.f;
			State = EAfterfallSprintState::Recovering;
			RecoveryLeft = RecoveryDuration;
		}
		break;
	case EAfterfallSprintState::Recovering:
		RecoveryLeft -= SimDelta;
		if (RecoveryLeft <= 0.f)
		{
			RecoveryLeft = 0.f;
			State = EAfterfallSprintState::Ready;
		}
		break;
	}
}

void UAfterfallMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	// Fixed-quantum simulation clock for gameplay timing (sprint/carry).
	// Rendering stays decoupled; the accumulator bounds catch-up work.
	const int32 Steps = ConsumeFixedSteps(StepAccumulator, (double)DeltaTime,
		(double)Tuning.FixedStepSec, Tuning.MaxStepsPerTick);
	const float SimDelta = Steps * Tuning.FixedStepSec;
	SimTimeSec += SimDelta;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const bool bWant = bWantSprint && !bCarryingHeavy;
		const float SprintDuration = Tuning.SprintDurationSec / (1.f + 0.2f * LoadRatio);
		AdvanceSprint(SprintState, SprintTimeLeft, RecoveryTimeLeft, bWant,
			SprintDuration, Tuning.SprintRecoverySec, SimDelta);
	}

	// Load ratio.
	LoadRatio = (ReferenceMassKg > 0.f) ? (CarriedMassKg / ReferenceMassKg) : 0.f;
	float AccelFactor, SpeedFactor, BrakeFactor;
	ComputeMovementFactors(LoadRatio, AccelFactor, SpeedFactor, BrakeFactor);

	const bool bSprinting = SprintState == EAfterfallSprintState::Sprinting;
	const float BaseAccel = bSprinting ? Tuning.SprintAcceleration : Tuning.NormalAcceleration;
	MaxAcceleration = BaseAccel * AccelFactor;
	EffectiveAcceleration = MaxAcceleration;

	float BaseSpeed = Tuning.NormalMaxSpeed;
	if (bCarryingHeavy)
	{
		BaseSpeed = FMath::Min(BaseSpeed, Tuning.CarryCapCmS);
	}
	if (SprintState == EAfterfallSprintState::Sprinting)
	{
		BaseSpeed = FMath::Max(BaseSpeed, Tuning.SprintMaxSpeed);
	}
	MaxWalkSpeed = BaseSpeed * SpeedFactor;
	EffectiveMaxSpeed = MaxWalkSpeed;

	BrakingDecelerationWalking = Tuning.BrakingDeceleration * BrakeFactor;
	EffectiveBraking = BrakingDecelerationWalking;
	GroundFriction = Tuning.GroundFriction;
	AirControl = Tuning.AirControl;

	if (CVarLoadDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Yellow, FString::Printf(TEXT("Mass=%.1f Speed=%.1f Accel=%.1f Brake=%.1f SprintMult=%.2f"), CarriedMassKg, MaxWalkSpeed, MaxAcceleration, BrakingDecelerationWalking, 1.f/(1.f+0.2f*LoadRatio)));
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

float UAfterfallMovementComponent::GetMaxSpeed() const
{
	if (bCarryingHeavy)
	{
		return Tuning.CarryCapCmS;
	}
	if (SprintState == EAfterfallSprintState::Sprinting)
	{
		return Tuning.SprintMaxSpeed;
	}
	return Tuning.NormalMaxSpeed;
}

FPortableEquation UAfterfallMovementComponent::AccelLoadEquation()
{
	return Afterfall::Portable::Linear(-0.15f, 1.f);
}

FPortableEquation UAfterfallMovementComponent::SpeedLoadEquation()
{
	return Afterfall::Portable::Linear(-0.08f, 1.f);
}

FPortableEquation UAfterfallMovementComponent::BrakeLoadEquation()
{
	return Afterfall::Portable::Linear(-0.10f, 1.f);
}

void UAfterfallMovementComponent::ComputeMovementFactors(float LoadRatio, float& OutAccelFactor, float& OutSpeedFactor, float& OutBrakeFactor)
{
	// Single evaluation point for the shared load-response equations:
	// identical values to the documented 1 - k*L lines (regression-guarded
	// by Afterfall.Load.Equations.*), now exported through FPortableEquation.
	const float L = FMath::Clamp(LoadRatio, 0.f, 4.f);
	OutAccelFactor = FMath::Clamp(Afterfall::Portable::Eval(AccelLoadEquation(), L), 0.3f, 1.f);
	OutSpeedFactor = FMath::Clamp(Afterfall::Portable::Eval(SpeedLoadEquation(), L), 0.5f, 1.f);
	OutBrakeFactor = FMath::Clamp(Afterfall::Portable::Eval(BrakeLoadEquation(), L), 0.4f, 1.f);
}

void UAfterfallMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAfterfallMovementComponent, SprintState);
	DOREPLIFETIME(UAfterfallMovementComponent, SprintTimeLeft);
	DOREPLIFETIME(UAfterfallMovementComponent, RecoveryTimeLeft);
	DOREPLIFETIME(UAfterfallMovementComponent, bCarryingHeavy);
	DOREPLIFETIME(UAfterfallMovementComponent, CarriedMassKg);
}
