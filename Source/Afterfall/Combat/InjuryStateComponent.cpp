// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "InjuryStateComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/MedicalCapability.h"
#include "Inventory/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

namespace
{
	constexpr int32 kPartCount = 6;
	constexpr float kBleedPerSeverity[4] = { 0.f, 0.5f, 1.5f, 3.0f };
}

UInjuryStateComponent::UInjuryStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	PartInjuries.SetNum(kPartCount);
	for (int32 i = 0; i < kPartCount; ++i)
	{
		PartInjuries[i].PartIndex = i;
	}
}

void UInjuryStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (PartInjuries.Num() != kPartCount)
	{
		PartInjuries.SetNum(kPartCount);
		for (int32 i = 0; i < kPartCount; ++i)
		{
			PartInjuries[i].PartIndex = i;
		}
	}
}

void UInjuryStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInjuryStateComponent, PartInjuries);
	DOREPLIFETIME(UInjuryStateComponent, TreatmentState);
	DOREPLIFETIME(UInjuryStateComponent, TreatingPartIndex);
	DOREPLIFETIME(UInjuryStateComponent, TreatingItemId);
	DOREPLIFETIME(UInjuryStateComponent, TreatmentProgress);
	DOREPLIFETIME(UInjuryStateComponent, PendingHPRestore);
}

void UInjuryStateComponent::OnRep_Treatment() {}

bool UInjuryStateComponent::CanTransition(ETreatmentState From, ETreatmentState To)
{
	switch (From)
	{
	case ETreatmentState::Idle:
		return To == ETreatmentState::Treating;
	case ETreatmentState::Treating:
		return To == ETreatmentState::Complete || To == ETreatmentState::Interrupted;
	case ETreatmentState::Complete:
	case ETreatmentState::Interrupted:
		return To == ETreatmentState::Idle || To == ETreatmentState::Treating;
	}
	return false;
}

int32 UInjuryStateComponent::PartToIndex(EBodyPart Part)
{
	// Must match EBodyPart declaration order (Head=0 .. LegR=5).
	switch (Part)
	{
	case EBodyPart::Head:  return 0;
	case EBodyPart::Torso: return 1;
	case EBodyPart::ArmL:  return 2;
	case EBodyPart::ArmR:  return 3;
	case EBodyPart::LegL:  return 4;
	case EBodyPart::LegR:  return 5;
	}
	return 0;
}

float UInjuryStateComponent::BleedTick(float RateHpS, float DeltaSeconds)
{
	if (RateHpS <= 0.f || DeltaSeconds <= 0.f)
	{
		return 0.f;
	}
	return RateHpS * DeltaSeconds;
}

UHealthComponent* UInjuryStateComponent::HealthOfOwner() const
{
	const AActor* O = GetOwner();
	return O ? O->FindComponentByClass<UHealthComponent>() : nullptr;
}

UInventoryComponent* UInjuryStateComponent::InventoryOfOwner() const
{
	const AActor* O = GetOwner();
	return O ? O->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UInjuryStateComponent::ServerApplyWound(EBodyPart Part, int32 Severity)
{
	AActor* O = GetOwner();
	if (!O || !O->HasAuthority())
	{
		return; // server-authoritative: clients never write wounds
	}
	if (PartInjuries.Num() != kPartCount)
	{
		return;
	}
	const int32 Idx = PartToIndex(Part);
	const int32 Sev = FMath::Clamp(Severity, 1, 3);
	FInjuryRecord& R = PartInjuries[Idx];
	R.bWounded = true;
	R.Severity = FMath::Max(R.Severity, Sev);
	// Bleeding starts on any wound unless already stopped; rate follows
	// the worst severity on the part.
	R.bBleeding = true;
	R.BleedRateHpS = kBleedPerSeverity[R.Severity];
	R.bPain = R.bPain || Sev >= 1;
}

void UInjuryStateComponent::ServerBeginTreatment(EBodyPart Part, FName ItemId)
{
	AActor* O = GetOwner();
	if (!O || !O->HasAuthority())
	{
		return;
	}
	if (PartInjuries.Num() != kPartCount)
	{
		return;
	}
	// Duplicate-request protection: exactly one session at a time.
	if (TreatmentState == ETreatmentState::Treating)
	{
		UE_LOG(LogAfterfall, Verbose, TEXT("Treatment rejected: session already active"));
		return;
	}
	const int32 Idx = PartToIndex(Part);
	const FInjuryRecord& R = PartInjuries[Idx];
	if (!R.bWounded)
	{
		UE_LOG(LogAfterfall, Verbose, TEXT("Treatment rejected: no wound on part %d"), Idx);
		return;
	}
	const FMedicalCapability* Cap = UMedicalCapabilityLib::FindCapability(ItemId);
	if (!Cap)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Treatment rejected: unknown item %s"), *ItemId.ToString());
		return;
	}
	if (!UMedicalCapabilityLib::CapabilityMatchesInjury(*Cap,
		R.bBleeding, R.bPain, R.Severity, R.bStabilized))
	{
		UE_LOG(LogAfterfall, Verbose, TEXT("Treatment rejected: %s cannot help part %d"),
			*ItemId.ToString(), Idx);
		return;
	}
	UHealthComponent* H = HealthOfOwner();
	if (H && H->bIsDead)
	{
		return; // death path owns the body now; ForceInterrupt via death hook
	}
	UInventoryComponent* Inv = InventoryOfOwner();
	if (!Inv || Inv->GetCount(ItemId) < 1)
	{
		UE_LOG(LogAfterfall, Verbose, TEXT("Treatment rejected: no %s held"), *ItemId.ToString());
		return;
	}
	TreatmentState = ETreatmentState::Treating;
	TreatingPartIndex = Idx;
	TreatingItemId = ItemId;
	TreatmentProgress = 0.f;
	TreatmentElapsed = 0.f;
	TreatmentDuration = FMath::Max(Cap->ApplicationSec, 0.5f);
	ActiveSessionId = FGuid::NewGuid();
}

void UInjuryStateComponent::ServerInterruptTreatment()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (TreatmentState != ETreatmentState::Treating)
	{
		return; // safe any time; non-sessions are no-ops
	}
	// Interrupt never consumes the item; progress is discarded.
	TreatmentState = ETreatmentState::Interrupted;
	TreatingPartIndex = -1;
	TreatingItemId = NAME_None;
	TreatmentProgress = 0.f;
	TreatmentElapsed = 0.f;
	TreatmentDuration = 0.f;
}

void UInjuryStateComponent::ServerAdvance(float DeltaSeconds)
{
	AActor* O = GetOwner();
	if (!O || !O->HasAuthority() || DeltaSeconds <= 0.f)
	{
		return;
	}
	if (PartInjuries.Num() != kPartCount)
	{
		return;
	}
	// Bleeding drains HP through the public HealthComponent entry point.
	// Each part bleeds independently; death latches inside HealthComponent.
	UHealthComponent* H = HealthOfOwner();
	if (H && !H->bIsDead)
	{
		float TotalBleed = 0.f;
		for (const FInjuryRecord& R : PartInjuries)
		{
			if (R.bWounded && R.bBleeding)
			{
				TotalBleed += BleedTick(R.BleedRateHpS, DeltaSeconds);
			}
		}
		if (TotalBleed > 0.f)
		{
			H->ServerTakeDamage(TotalBleed);
		}
	}
	// Treatment progress; completion applies effects atomically.
	if (TreatmentState == ETreatmentState::Treating)
	{
		// Death or owner loss aborts without consuming.
		if (!H || H->bIsDead)
		{
			ServerInterruptTreatment();
			return;
		}
		TreatmentElapsed += DeltaSeconds;
		TreatmentProgress = FMath::Clamp(TreatmentElapsed / TreatmentDuration, 0.f, 1.f);
		if (TreatmentElapsed >= TreatmentDuration)
		{
			const FMedicalCapability* Cap = UMedicalCapabilityLib::FindCapability(TreatingItemId);
			UInventoryComponent* Inv = InventoryOfOwner();
			if (Cap && Inv && Inv->ServerTryConsumeItem(TreatingItemId, 1))
			{
				FInjuryRecord& R = PartInjuries[TreatingPartIndex];
				if (Cap->bStopsBleeding)
				{
					R.bBleeding = false;
					R.BleedRateHpS = 0.f;
				}
				if (Cap->bRelievesPain)
				{
					R.bPain = false;
				}
				if (Cap->bStabilizes)
				{
					R.bStabilized = true;
				}
				if (Cap->HPRestore > 0.f)
				{
					// Phase 2 owner hook consumes this via ConsumePendingHeal.
					PendingHPRestore += Cap->HPRestore;
				}
				// A fully dressed, non-bleeding light wound closes; severe
				// wounds persist (stabilized) until further care.
				if (!R.bBleeding && R.Severity < 2)
				{
					R.bWounded = false;
					R.Severity = 0;
				}
				TreatmentState = ETreatmentState::Complete;
				// Phase 2 heal hook: the item's HP restore reaches the canonical
				// health pool exactly once, through its server entry point.
				if (PendingHPRestore > 0.f)
				{
					if (UHealthComponent* HealTarget = HealthOfOwner())
					{
						HealTarget->ServerHeal(ConsumePendingHeal());
					}
				}
			}
			else
			{
				// Item vanished mid-treatment (transfer/concurrent use):
				// no effects, no consumption, back to Idle via interrupt.
				ServerInterruptTreatment();
			}
			TreatingPartIndex = -1;
			TreatingItemId = NAME_None;
			TreatmentProgress = TreatmentState == ETreatmentState::Complete ? 1.f : 0.f;
		}
	}
}

float UInjuryStateComponent::ConsumePendingHeal()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return 0.f;
	}
	const float V = PendingHPRestore;
	PendingHPRestore = 0.f;
	return V;
}

void UInjuryStateComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* O = GetOwner();
	if (!O || !O->HasAuthority())
	{
		return; // server simulates; clients replicate
	}
	ServerAdvance(DeltaTime);
}
