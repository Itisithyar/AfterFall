#include "Items/ItemRegistry.h"
#include "Engine/AssetManager.h"

void UAfterfallItemRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (TObjectIterator<UAfterfallItemDefinition> It; It; ++It)
	{
		RegisterItem(*It);
	}
}

void UAfterfallItemRegistry::RegisterItem(UAfterfallItemDefinition* Def)
{
	if (!Def || Def->ItemID.IsNone()) return;
	NodesByID.Add(Def->ItemID, Def);
	for (FName P : Def->ParentIDs)
	{
		ChildrenByParent.FindOrAdd(P).AddUnique(Def->ItemID);
	}
}

UAfterfallItemDefinition* UAfterfallItemRegistry::GetItem(FName ID) const
{
	if (const UAfterfallItemDefinition* const* P = NodesByID.Find(ID)) return const_cast<UAfterfallItemDefinition*>(*P);
	return nullptr;
}

TArray<UAfterfallItemDefinition*> UAfterfallItemRegistry::GetChildren(FName ID) const
{
	TArray<UAfterfallItemDefinition*> Out;
	if (const TArray<FName>* Kids = ChildrenByParent.Find(ID))
		for (FName K : *Kids) if (UAfterfallItemDefinition* D = GetItem(K)) Out.Add(D);
	return Out;
}

UAfterfallItemDefinition* UAfterfallItemRegistry::TryCombine(FName A, FName B) const
{
	for (const auto& Pair : NodesByID)
	{
		if (Pair.Value->ParentIDs.Num() == 2 && Pair.Value->ParentIDs.Contains(A) && Pair.Value->ParentIDs.Contains(B))
			return Pair.Value;
	}
	return nullptr;
}
