// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Persistence) — versioned snapshot of the authoritative
// inventory grid, including per-instance lineage. Deliberately file/format
// helpers only: no gameplay logic, no world access. The region delta system
// (FRegionStateSave) owns world state; this owns the inventory container
// state, so both stay in their existing architecture lanes.
#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTypes.h"
#include "UObject/NoExportTypes.h"
#include "InventorySave.generated.h"

/**
 * One inventory owner's full slot grid as persisted. OwnerId ties the
 * snapshot to the pawn/container it came from so a reload can reattach it
 * instead of merging it into the wrong owner (ownership = one authoritative
 * container per instance).
 */
USTRUCT(BlueprintType)
struct FInventorySave
{
	GENERATED_BODY()

	/** Snapshot format version (see UInventoryPersistence::CurrentSaveVersion). */
	UPROPERTY()
	int32 SaveVersion = 1;

	/** Stable key of the owning pawn/container ("" = unowned snapshot). */
	UPROPERTY()
	FString OwnerId;

	/** Full slot grid: identity, counts, condition, lineage all included. */
	UPROPERTY()
	TArray<FInventorySlot> Slots;
};

/**
 * Binary, versioned inventory serialization.
 *
 * Compatibility policy (via UPersistenceService::IsSaveCompatible):
 *  - file version == code version: exact load;
 *  - file version older: load + migration flag (never silently drops fields);
 *  - file version newer: reject (never parse unknown future fields as v1);
 *  - bad magic / truncated / trailing bytes: reject with an error.
 */
UCLASS()
class AFTERFALL_API UInventoryPersistence : public UObject
{
	GENERATED_BODY()

public:
	/** Current code-side snapshot version. Bump when fields are added. */
	static constexpr int32 CurrentSaveVersion = 1;

	/** 'AFFS' little-endian: rejects non-save payloads up front. */
	static constexpr uint32 SaveMagic = 0x53464641u;

	/** Serialize to bytes (header + slots). Never fails. Named ToBytes (not
	 *  Serialize) so it never hides UObject::Serialize(FArchive&). */
	static TArray<uint8> ToBytes(const FInventorySave& Save);

	/** Parse bytes. Returns false + OutError on any incompatibility. */
	static bool Deserialize(const TArray<uint8>& Bytes, FInventorySave& OutSave, FString& OutError);

	static bool SaveToFile(const FInventorySave& Save, const FString& AbsolutePath);
	static bool LoadFromFile(const FString& AbsolutePath, FInventorySave& OutSave, FString& OutError);
};
