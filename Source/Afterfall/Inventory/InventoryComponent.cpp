// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

UInventoryComponent::UInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	Wood = 10;
	// Seed slice kit: mirrors legacy Wood for migration period.
	Items.Add({ TEXT("AF_R_Wood"), 10 });
}

int32 UInventoryComponent::GetCount(FName ItemId) const
{
	for (const FInventoryEntry& E : Items)
	{
		if (E.ItemId == ItemId)
		{
			return E.Count;
		}
	}
	return 0;
}

float UInventoryComponent::WeightForItem(FName ItemId)
{
	// Slice weights (kg). Full table moves to DT_Items in Phase B.
	static const TMap<FName, float> W = {
		{ TEXT("AF_R_Wood"), 0.5f }, { TEXT("res_wood_log"), 0.5f },
		{ TEXT("AF_R_Stone"), 0.8f }, { TEXT("AF_R_HempFiber"), 0.1f },
		{ TEXT("AF_R_Scrap"), 1.0f }, { TEXT("res_scrap_metal"), 1.0f },
		{ TEXT("AF_M_Plank"), 1.2f }, { TEXT("mat_plank"), 1.2f },
		{ TEXT("AF_M_Rope"), 0.4f }, { TEXT("mat_rope"), 0.4f },
		{ TEXT("AF_M_Cloth"), 0.3f }, { TEXT("AF_M_Fasteners"), 0.1f },
		{ TEXT("mat_nail"), 0.05f }, { TEXT("AF_F_Canned"), 0.4f },
		{ TEXT("AF_MED_Bandage"), 0.1f }, { TEXT("res_med_bandage"), 0.1f },
	};
	if (const float* F = W.Find(ItemId))
	{
		return *F;
	}
	return 1.0f;
}

float UInventoryComponent::GetWeightKg() const
{
	float Total = 0.f;
	for (const FInventoryEntry& E : Items)
	{
		Total += WeightForItem(E.ItemId) * E.Count;
	}
	return Total;
}

int32* UInventoryComponent::FindEntry(FName ItemId)
{
	for (FInventoryEntry& E : Items)
	{
		if (E.ItemId == ItemId)
		{
			return &E.Count;
		}
	}
	return nullptr;
}

bool UInventoryComponent::ServerAddResource_Validate(int32 Amount)
{
	return Amount > 0 && Amount <= 100;
}

void UInventoryComponent::ServerAddResource_Implementation(int32 Amount)
{
	Wood = FMath::Clamp(Wood + Amount, 0, 9999);
	// Keep map in sync during migration.
	if (int32* C = FindEntry(TEXT("AF_R_Wood")))
	{
		*C = FMath::Clamp(*C + Amount, 0, 9999);
	}
	else
	{
		Items.Add({ TEXT("AF_R_Wood"), Amount });
	}
}

bool UInventoryComponent::ServerAddItem_Validate(FName ItemId, int32 Count)
{
	return !ItemId.IsNone() && Count > 0 && Count <= 100;
}

void UInventoryComponent::ServerAddItem_Implementation(FName ItemId, int32 Count)
{
	const float NewWeight = GetWeightKg() + WeightForItem(ItemId) * Count;
	if (NewWeight > MaxWeightKg + 0.001f)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("AddItem rejected: overweight %.1f/%.1f"), NewWeight, MaxWeightKg);
		return;
	}
	if (int32* C = FindEntry(ItemId))
	{
		*C = FMath::Clamp(*C + Count, 0, 9999);
	}
	else
	{
		Items.Add({ ItemId, Count });
	}
	if (ItemId == TEXT("AF_R_Wood") || ItemId == TEXT("res_wood_log"))
	{
		Wood = GetCount(TEXT("AF_R_Wood")) + GetCount(TEXT("res_wood_log"));
	}
}

bool UInventoryComponent::ServerTryConsume(int32 Cost)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (Cost <= 0)
	{
		return false;
	}
	// Prefer generic AF_R_Wood entry, fall back to legacy int.
	if (int32* C = FindEntry(TEXT("AF_R_Wood")))
	{
		if (*C < Cost)
		{
			return false;
		}
		*C -= Cost;
		Wood = *C + GetCount(TEXT("res_wood_log"));
		return true;
	}
	if (Wood < Cost)
	{
		return false;
	}
	Wood -= Cost;
	return true;
}

bool UInventoryComponent::ServerTryConsumeItem(FName ItemId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ServerTryConsumeItem without authority — rejected"));
		return false;
	}
	if (ItemId.IsNone() || Count <= 0)
	{
		return false;
	}
	if (int32* C = FindEntry(ItemId))
	{
		if (*C < Count)
		{
			return false;
		}
		*C -= Count;
		if (*C <= 0)
		{
			Items.RemoveAll([&](const FInventoryEntry& E) { return E.ItemId == ItemId; });
		}
		return true;
	}
	return false;
}

void UInventoryComponent::OnRep_Wood() {}
void UInventoryComponent::OnRep_Items() {}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInventoryComponent, Wood);
	DOREPLIFETIME(UInventoryComponent, Items);
}
