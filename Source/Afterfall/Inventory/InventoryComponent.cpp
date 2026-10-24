// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Inventory/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

UInventoryComponent::UInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	Wood = 10; // vertical-slice starting kit
}

bool UInventoryComponent::ServerAddResource_Validate(int32 Amount)
{
	return Amount > 0 && Amount <= 100;
}

void UInventoryComponent::ServerAddResource_Implementation(int32 Amount)
{
	Wood = FMath::Clamp(Wood + Amount, 0, 9999);
	UE_LOG(LogAfterfall, Log, TEXT("Inventory grant +%d (now %d)"), Amount, Wood);
}

bool UInventoryComponent::ServerTryConsume(int32 Cost)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ServerTryConsume called without authority — rejected"));
		return false;
	}
	if (Cost <= 0 || Wood < Cost)
	{
		return false;
	}
	Wood -= Cost;
	return true;
}

void UInventoryComponent::OnRep_Wood()
{
	// HUD update hook (Phase 5).
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInventoryComponent, Wood);
}
