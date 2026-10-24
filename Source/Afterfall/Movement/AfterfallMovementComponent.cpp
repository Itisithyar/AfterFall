// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallMovementComponent.h"
#include "Net/UnrealNetwork.h"

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
		AdvanceSprint(SprintState, SprintTimeLeft, RecoveryTimeLeft, bWant,
			Tuning.SprintDurationSec, Tuning.SprintRecoverySec, SimDelta);
	}

	// Per-state momentum parameters (engine still owns collision integration).
	const bool bSprinting = SprintState == EAfterfallSprintState::Sprinting;
	MaxAcceleration = bSprinting ? Tuning.SprintAcceleration : Tuning.NormalAcceleration;
	BrakingDecelerationWalking = Tuning.BrakingDeceleration;
	GroundFriction = Tuning.GroundFriction;
	AirControl = Tuning.AirControl;

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

void UAfterfallMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAfterfallMovementComponent, SprintState);
	DOREPLIFETIME(UAfterfallMovementComponent, SprintTimeLeft);
	DOREPLIFETIME(UAfterfallMovementComponent, RecoveryTimeLeft);
	DOREPLIFETIME(UAfterfallMovementComponent, bCarryingHeavy);
}
