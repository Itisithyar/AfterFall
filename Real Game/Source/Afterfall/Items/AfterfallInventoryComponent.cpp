#include "Items/AfterfallInventoryComponent.h"
#include "Engine/GameInstance.h"

int32 UAfterfallInventoryComponent::AddItem(FName ID, int32 Qty)
{
	if (Qty <= 0) return 0;
	UAfterfallItemRegistry* Reg = GetOwner() ? GetOwner()->GetGameInstance()->GetSubsystem<UAfterfallItemRegistry>() : nullptr;
	int32 MaxStack = 0;
	if (Reg) if (UAfterfallItemDefinition* Def = Reg->GetItem(ID)) MaxStack = Def->MaxStackSize;
	// find existing slot
	for (FItemStack& S : Slots)
	{
		if (S.ItemID != ID) continue;
		if (MaxStack > 0 && S.Quantity >= MaxStack) continue;
		int32 Space = (MaxStack > 0) ? (MaxStack - S.Quantity) : Qty;
		int32 Add = FMath::Min(Space, Qty);
		S.Quantity += Add; Qty -= Add;
		if (Qty <= 0) { OnInventoryChanged.Broadcast(); return 0; }
	}
	// new stack
	while (Qty > 0)
	{
		int32 Add = (MaxStack > 0) ? FMath::Min(MaxStack, Qty) : Qty;
		FItemStack S; S.ItemID = ID; S.Quantity = Add; Slots.Add(S); Qty -= Add;
		if (MaxStack == 0) break;
	}
	OnInventoryChanged.Broadcast();
	return Qty;
}

bool UAfterfallInventoryComponent::RemoveItem(FName ID, int32 Qty)
{
	for (int32 i = Slots.Num()-1; i >= 0 && Qty > 0; i--)
	{
		if (Slots[i].ItemID != ID) continue;
		int32 Take = FMath::Min(Slots[i].Quantity, Qty);
		Slots[i].Quantity -= Take; Qty -= Take;
		if (Slots[i].Quantity <= 0) Slots.RemoveAt(i);
	}
	OnInventoryChanged.Broadcast();
	return Qty == 0;
}

bool UAfterfallInventoryComponent::HasItem(FName ID, int32 Qty) const
{
	int32 Total = 0;
	for (const FItemStack& S : Slots) if (S.ItemID == ID) Total += S.Quantity;
	return Total >= Qty;
}

void UAfterfallInventoryComponent::SwapSlots(int32 IndexA, int32 IndexB)
{
	if (IndexA == IndexB || !Slots.IsValidIndex(IndexA) || !Slots.IsValidIndex(IndexB))
	{
		return; // out of range: leave the mat exactly as it was
	}
	Slots.Swap(IndexA, IndexB);
	OnInventoryChanged.Broadcast();
}

bool UAfterfallInventoryComponent::TryCombineItems(FName A, FName B)
{
	UAfterfallItemRegistry* Reg = GetOwner() ? GetOwner()->GetGameInstance()->GetSubsystem<UAfterfallItemRegistry>() : nullptr;
	if (!Reg) return false;
	UAfterfallItemDefinition* Child = Reg->TryCombine(A, B);
	if (!Child) return false;
	if (!RemoveItem(A,1) || !RemoveItem(B,1)) return false;
	AddItem(Child->ItemID, 1);
	return true;
}
