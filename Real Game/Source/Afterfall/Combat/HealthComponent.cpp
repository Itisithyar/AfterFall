// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

UHealthComponent::UHealthComponent()
{
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::ServerTakeDamage(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsDead)
	{
		return;
	}
	const float Clamped = ClampDamage(Amount);
	if (Clamped <= 0.f)
	{
		return;
	}
	HP = FMath::Max(HP - Clamped, 0.f);
	UE_LOG(LogAfterfall, Verbose, TEXT("Damage %.0f -> %s HP %.0f"),
		Clamped, *GetOwner()->GetName(), HP);
	if (HP <= 0.f)
	{
		bIsDead = true;
		OnHealthDepleted.Broadcast();
	}
}

void UHealthComponent::ServerRevive()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	HP = MaxHP;
	bIsDead = false;
}

void UHealthComponent::ServerHeal(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsDead)
	{
		return;
	}
	HP = FMath::Clamp(HP + Amount, 0.f, MaxHP);
}

void UHealthComponent::OnRep_HP() {}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, HP);
	DOREPLIFETIME(UHealthComponent, bIsDead);
}
