#include "Items/LootTable.h"
#include "Items/ItemRegistry.h"
#include "Engine/GameInstance.h"

TArray<FItemStack> UAfterfallLootTable::RollLoot(int32 NumRolls, int32 Seed) const
{
	TArray<FItemStack> Out;
	FRandomStream R(Seed);
	for (int32 i = 0; i < NumRolls; i++)
	{
		float Total = 0; for (const FLootEntry& E : Entries) Total += E.Weight;
		float Pick = R.FRandRange(0.f, Total);
		float Acc = 0;
		for (const FLootEntry& E : Entries)
		{
			Acc += E.Weight;
			if (Pick <= Acc)
			{
				int32 Qty = R.RandRange(E.MinQty, E.MaxQty);
				FItemStack S; S.ItemID = E.ItemID; S.Quantity = Qty; Out.Add(S);
				break;
			}
		}
	}
	return Out;
}

void UAfterfallLootTable::Validate() const
{
	// validation left to registry at runtime
}
