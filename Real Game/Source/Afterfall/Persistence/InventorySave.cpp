// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Persistence) — versioned inventory snapshot IO.
#include "Persistence/InventorySave.h"
#include "Persistence/PersistenceService.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Afterfall.h"

namespace
{
	// FArchive Guid ops take non-const refs; work on copies so const
	// snapshot data is never touched.
	void WriteGuid(FArchive& Ar, const FGuid& G)
	{
		FGuid Tmp = G;
		Ar << Tmp;
	}

	FGuid ReadGuid(FArchive& Ar)
	{
		FGuid Tmp;
		Ar << Tmp;
		return Tmp;
	}

	void WriteFString(FArchive& Ar, const FString& S)
	{
		FString Tmp = S;
		Ar << Tmp;
	}

	FString ReadFString(FArchive& Ar)
	{
		FString Tmp;
		Ar << Tmp;
		return Tmp;
	}

	// Sanity bounds: a corrupt or hostile payload must not allocate wildly.
	constexpr int32 MaxSlots = 4096;
	constexpr int32 MaxParents = 1024;

	void WriteItem(FArchive& Ar, const FItemInstance& Item)
	{
		WriteFString(Ar, Item.ItemId.ToString());
		WriteGuid(Ar, Item.InstanceId);
		Ar << const_cast<int32&>(Item.Count);
		Ar << const_cast<float&>(Item.Condition);
		WriteFString(Ar, Item.CustomData);

		int32 NumParents = Item.ParentInstanceIds.Num();
		Ar << NumParents;
		for (const FGuid& P : Item.ParentInstanceIds)
		{
			WriteGuid(Ar, P);
		}
		WriteFString(Ar, Item.ProducedByRecipe.ToString());
		Ar << const_cast<int32&>(Item.RecipeVersion);
		WriteGuid(Ar, Item.CreationTransactionId);
		Ar << const_cast<int32&>(Item.SchemaVersion);
	}

	bool ReadItem(FArchive& Ar, FItemInstance& OutItem)
	{
		const FString IdStr = ReadFString(Ar);
		OutItem.ItemId = FName(*IdStr);
		OutItem.InstanceId = ReadGuid(Ar);
		Ar << OutItem.Count;
		Ar << OutItem.Condition;
		OutItem.CustomData = ReadFString(Ar);

		int32 NumParents = 0;
		Ar << NumParents;
		if (NumParents < 0 || NumParents > MaxParents)
		{
			return false;
		}
		OutItem.ParentInstanceIds.Reset();
		for (int32 i = 0; i < NumParents; ++i)
		{
			OutItem.ParentInstanceIds.Add(ReadGuid(Ar));
		}
		OutItem.ProducedByRecipe = FName(*ReadFString(Ar));
		Ar << OutItem.RecipeVersion;
		OutItem.CreationTransactionId = ReadGuid(Ar);
		Ar << OutItem.SchemaVersion;

		if (Ar.IsError() || OutItem.Count < 0 || OutItem.Count > 1000000)
		{
			return false;
		}
		return true;
	}
}

TArray<uint8> UInventoryPersistence::ToBytes(const FInventorySave& Save)
{
	TArray<uint8> Bytes;
	FMemoryWriter Ar(Bytes);

	uint32 Magic = SaveMagic;
	Ar << Magic;
	int32 Version = Save.SaveVersion;
	Ar << Version;
	WriteFString(Ar, Save.OwnerId);

	int32 SlotCount = Save.Slots.Num();
	Ar << SlotCount;
	for (const FInventorySlot& S : Save.Slots)
	{
		int32 SlotIndex = S.SlotIndex;
		Ar << SlotIndex;
		uint8 Locked = S.bLocked ? 1 : 0;
		Ar << Locked;
		int32 Capacity = S.Capacity;
		Ar << Capacity;
		uint8 HasItem = S.IsEmpty() ? 0 : 1;
		Ar << HasItem;
		if (HasItem)
		{
			WriteItem(Ar, S.Item);
		}
	}
	return Bytes;
}

bool UInventoryPersistence::Deserialize(const TArray<uint8>& Bytes, FInventorySave& OutSave, FString& OutError)
{
	OutSave = FInventorySave();
	OutError.Reset();

	FMemoryReader Ar(Bytes);
	uint32 Magic = 0;
	Ar << Magic;
	if (Ar.IsError() || Magic != SaveMagic)
	{
		OutError = TEXT("not an AFTERFALL inventory save (bad magic)");
		return false;
	}
	int32 Version = 0;
	Ar << Version;
	if (Version < 1)
	{
		OutError = FString::Printf(TEXT("invalid save version %d"), Version);
		return false;
	}
	bool bNeedsMigration = false;
	if (!UPersistenceService::IsSaveCompatible(Version, CurrentSaveVersion, bNeedsMigration))
	{
		OutError = FString::Printf(TEXT("save version %d newer than code version %d — rejected"),
			Version, CurrentSaveVersion);
		return false;
	}
	if (bNeedsMigration)
	{
		UE_LOG(LogAfterfall, Log, TEXT("Inventory save v%d migrated forward to v%d"),
			Version, CurrentSaveVersion);
	}

	OutSave.SaveVersion = Version;
	OutSave.OwnerId = ReadFString(Ar);

	int32 SlotCount = 0;
	Ar << SlotCount;
	if (Ar.IsError() || SlotCount < 0 || SlotCount > MaxSlots)
	{
		OutError = TEXT("corrupt slot table");
		return false;
	}
	OutSave.Slots.Reserve(SlotCount);
	for (int32 i = 0; i < SlotCount; ++i)
	{
		FInventorySlot S;
		Ar << S.SlotIndex;
		uint8 Locked = 0;
		Ar << Locked;
		S.bLocked = Locked != 0;
		Ar << S.Capacity;
		uint8 HasItem = 0;
		Ar << HasItem;
		if (HasItem && !ReadItem(Ar, S.Item))
		{
			OutError = TEXT("corrupt item instance record");
			return false;
		}
		OutSave.Slots.Add(MoveTemp(S));
	}
	if (Ar.IsError())
	{
		OutError = TEXT("truncated inventory save");
		return false;
	}
	if (Ar.Tell() != Bytes.Num())
	{
		// Never silently ignore unread data — it would be lost state.
		OutError = FString::Printf(TEXT("unread trailing data (%d bytes)"), Bytes.Num() - (int32)Ar.Tell());
		return false;
	}
	return true;
}

bool UInventoryPersistence::SaveToFile(const FInventorySave& Save, const FString& AbsolutePath)
{
	const TArray<uint8> Bytes = ToBytes(Save);
	return FFileHelper::SaveArrayToFile(Bytes, *AbsolutePath);
}

bool UInventoryPersistence::LoadFromFile(const FString& AbsolutePath, FInventorySave& OutSave, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("cannot read %s"), *AbsolutePath);
		return false;
	}
	return Deserialize(Bytes, OutSave, OutError);
}
