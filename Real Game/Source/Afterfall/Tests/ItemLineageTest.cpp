// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Tests) — vertical-slice acceptance for the contextual
// inventory / item lineage / loot task.
//
// Covers: unique child identity, parent-child lineage, weakest-link
// condition inheritance, transactional failure (byte-identical inventory on
// every rejection path), idempotent transaction replay, weight-budget
// rejection, deterministic station recipe resolution, versioned save/load
// round-trip (incl. incompatibility rejection), and loot anti-duplication.
//
// All tests are headless: a transient UInventoryComponent / ULootSystem with
// slots filled directly — no world, no input device, no cooked assets.
#include "Misc/AutomationTest.h"
#include "Inventory/InventoryComponent.h"
#include "Crafting/CraftingService.h"
#include "Persistence/InventorySave.h"
#include "Persistence/PersistenceService.h"
#include "Loot/LootSystem.h"
#include "Loot/LootService.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UInventoryComponent* MakeInventory()
	{
		return NewObject<UInventoryComponent>(GetTransientPackage());
	}

	/** Place an instance in the first free slot; returns its identity. */
	FGuid PutSlot(UInventoryComponent* Inv, FName ItemId, int32 Count, float Condition = 1.f)
	{
		for (FInventorySlot& S : Inv->Slots)
		{
			if (S.IsEmpty() && !S.bLocked)
			{
				S.Item = FItemInstance(ItemId, Count);
				S.Item.Condition = Condition;
				return S.Item.InstanceId;
			}
		}
		return FGuid();
	}

	const FInventorySlot* FindSlotByGuid(const UInventoryComponent* Inv, FGuid Id)
	{
		for (const FInventorySlot& S : Inv->Slots)
		{
			if (!S.IsEmpty() && S.Item.InstanceId == Id)
			{
				return &S;
			}
		}
		return nullptr;
	}

	const FInventorySlot* FindSlotByItemId(const UInventoryComponent* Inv, FName ItemId)
	{
		for (const FInventorySlot& S : Inv->Slots)
		{
			if (!S.IsEmpty() && S.Item.ItemId == ItemId)
			{
				return &S;
			}
		}
		return nullptr;
	}

	/** Byte-exact inventory fingerprint (via the production serializer). */
	TArray<uint8> Snapshot(UInventoryComponent* Inv)
	{
		FInventorySave S;
		S.Slots = Inv->Slots;
		return UInventoryPersistence::ToBytes(S);
	}
}

// ---------------------------------------------------------------------------
// Child identity + lineage + weakest-link condition
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageChildIdentityTest, "Afterfall.Lineage.ChildIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageChildIdentityTest::RunTest(const FString& P)
{
	// --- Partial consumption keeps the parent stack's identity.
	UInventoryComponent* Inv = MakeInventory();
	const FGuid Logs = PutSlot(Inv, TEXT("res_wood_log"), 3, 0.9f);
	FString Err;
	const FGuid Tx1 = FGuid::NewGuid();
	TestTrue(TEXT("plank combine commits"), Inv->TryCombine({ Logs }, TEXT("plank"), Tx1, Err));
	if (TestNotNull(TEXT("parent stack still present"), FindSlotByGuid(Inv, Logs)))
	{
		TestEqual(TEXT("partial consume leaves remainder"), FindSlotByGuid(Inv, Logs)->Item.Count, 1);
	}

	const FInventorySlot* Child = FindSlotByItemId(Inv, TEXT("mat_plank"));
	if (!TestNotNull(TEXT("child slot exists"), Child))
	{
		return false;
	}
	TestTrue(TEXT("child has fresh identity"), Child->Item.InstanceId != Logs);
	TestEqual(TEXT("child count"), Child->Item.Count, 1);
	TestEqual(TEXT("one parent recorded"), Child->Item.ParentInstanceIds.Num(), 1);
	if (Child->Item.ParentInstanceIds.Num() == 1)
	{
		TestTrue(TEXT("parent id matches consumed stack"), Child->Item.ParentInstanceIds[0] == Logs);
	}
	TestEqual(TEXT("rule id recorded"), Child->Item.ProducedByRecipe, FName(TEXT("plank")));
	TestEqual(TEXT("rule version recorded"), Child->Item.RecipeVersion, 1);
	TestTrue(TEXT("transaction id recorded"), Child->Item.CreationTransactionId == Tx1);
	TestTrue(TEXT("condition inherits from parent"),
		FMath::IsNearlyEqual(Child->Item.Condition, 0.9f, 0.0001f));
	TestTrue(TEXT("provenance string present"), Child->Item.CustomData.Contains(TEXT("made_by=plank")));

	// --- Multi-input combine: all parents recorded, weakest-link condition.
	UInventoryComponent* Inv2 = MakeInventory();
	const FGuid L = PutSlot(Inv2, TEXT("res_wood_log"), 3, 0.8f);
	const FGuid St = PutSlot(Inv2, TEXT("res_stone"), 3, 0.7f);
	const FGuid Fb = PutSlot(Inv2, TEXT("res_hemp_fiber"), 2, 0.4f);
	const FGuid Tx2 = FGuid::NewGuid();
	FString Err2;
	TestTrue(TEXT("hatchet combine commits"),
		Inv2->TryCombine({ L, St, Fb }, TEXT("hatchet"), Tx2, Err2));
	const FInventorySlot* Hatchet = FindSlotByItemId(Inv2, TEXT("tool_hatchet"));
	if (!TestNotNull(TEXT("hatchet exists"), Hatchet))
	{
		return false;
	}
	TestEqual(TEXT("all three parents recorded"), Hatchet->Item.ParentInstanceIds.Num(), 3);
	if (Hatchet->Item.ParentInstanceIds.Num() == 3)
	{
		TestTrue(TEXT("parent 1"), Hatchet->Item.ParentInstanceIds[0] == L);
		TestTrue(TEXT("parent 2"), Hatchet->Item.ParentInstanceIds[1] == St);
		TestTrue(TEXT("parent 3"), Hatchet->Item.ParentInstanceIds[2] == Fb);
	}
	TestTrue(TEXT("weakest-link (0.4), never averaged"),
		FMath::IsNearlyEqual(Hatchet->Item.Condition, 0.4f, 0.0001f));
	TestNull(TEXT("logs fully consumed"), FindSlotByGuid(Inv2, L));
	TestNull(TEXT("stone fully consumed"), FindSlotByGuid(Inv2, St));
	TestNull(TEXT("fiber fully consumed"), FindSlotByGuid(Inv2, Fb));
	return true;
}

// ---------------------------------------------------------------------------
// Every rejection path leaves the inventory byte-identical (no partial
// consumption, no silent drops, no minted children).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageRejectedRequestsTest, "Afterfall.Lineage.RejectedRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageRejectedRequestsTest::RunTest(const FString& P)
{
	UInventoryComponent* Inv = MakeInventory();
	const FGuid Logs = PutSlot(Inv, TEXT("res_wood_log"), 3, 1.f);
	const FGuid Fib = PutSlot(Inv, TEXT("res_hemp_fiber"), 2, 1.f);
	const TArray<uint8> Before = Snapshot(Inv);
	FString Err;

	TestFalse(TEXT("invalid transaction id rejected"),
		Inv->TryCombine({ Logs }, TEXT("plank"), FGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("invalid transaction")));

	TestFalse(TEXT("unknown recipe rejected"),
		Inv->TryCombine({ Logs }, TEXT("no_such_recipe"), FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("unknown recipe")));

	TestFalse(TEXT("duplicate instance id rejected"),
		Inv->TryCombine({ Logs, Logs }, TEXT("plank"), FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("supplied twice")));

	TestFalse(TEXT("unknown instance id rejected"),
		Inv->TryCombine({ FGuid::NewGuid() }, TEXT("plank"), FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("not in inventory")));

	// cloth needs 3 fiber, only 2 supplied: one short.
	TestFalse(TEXT("insufficient quantity rejected"),
		Inv->TryCombine({ Fib }, TEXT("cloth"), FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("missing input")));

	// Plank consumes logs only: the supplied fiber would go unused.
	TestFalse(TEXT("unused supplied instance rejected"),
		Inv->TryCombine({ Logs, Fib }, TEXT("plank"), FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("not required")));

	const TArray<uint8> After = Snapshot(Inv);
	TestEqual(TEXT("inventory byte-identical after all rejections"),
		After.Num(), Before.Num());
	TestTrue(TEXT("inventory content identical after all rejections"), After == Before);
	return true;
}

// ---------------------------------------------------------------------------
// Transaction replay with the same id can never mint or consume again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageTransactionReplayTest, "Afterfall.Lineage.TransactionReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageTransactionReplayTest::RunTest(const FString& P)
{
	UInventoryComponent* Inv = MakeInventory();
	const FGuid Logs = PutSlot(Inv, TEXT("res_wood_log"), 4, 1.f);
	const FGuid Tx = FGuid::NewGuid();
	FString Err;

	TestTrue(TEXT("first combine commits"), Inv->TryCombine({ Logs }, TEXT("plank"), Tx, Err));
	const TArray<uint8> AfterFirst = Snapshot(Inv);

	// Replay the exact same transaction: rejected before touching state.
	TestFalse(TEXT("replayed transaction rejected"),
		Inv->TryCombine({ Logs }, TEXT("plank"), Tx, Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("duplicate transaction")));
	const TArray<uint8> AfterReplay = Snapshot(Inv);
	TestTrue(TEXT("replay changed nothing"), AfterReplay == AfterFirst);

	// A fresh transaction over the same inputs is a legitimate new combine
	// (2 logs remain): exactly one new child, never two from one request.
	TestTrue(TEXT("new transaction over remaining inputs commits"),
		Inv->TryCombine({ Logs }, TEXT("plank"), FGuid::NewGuid(), Err));
	int32 Planks = 0;
	for (const FInventorySlot& S : Inv->Slots)
	{
		if (!S.IsEmpty() && S.Item.ItemId == TEXT("mat_plank"))
		{
			++Planks;
		}
	}
	TestEqual(TEXT("exactly two children from two committed transactions"), Planks, 2);
	return true;
}

// ---------------------------------------------------------------------------
// Weight budget: rejection happens before commit; success after raising cap.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageWeightBudgetTest, "Afterfall.Lineage.WeightBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageWeightBudgetTest::RunTest(const FString& P)
{
	UInventoryComponent* Inv = MakeInventory();
	const FGuid Logs = PutSlot(Inv, TEXT("res_wood_log"), 2, 1.f); // 1.0 kg
	Inv->MaxWeightKg = 1.1f;
	const TArray<uint8> Before = Snapshot(Inv);
	FString Err;

	// Consuming 1.0 kg of logs to mint a 1.2 kg plank overshoots the cap.
	TestFalse(TEXT("overweight combine rejected"),
		Inv->TryCombine({ Logs }, TEXT("plank"), FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("overweight")));
	TestTrue(TEXT("nothing consumed or minted"), Snapshot(Inv) == Before);

	// Same request succeeds once the budget allows it: the earlier refusal
	// was the weight rule, not a logic fault.
	Inv->MaxWeightKg = 30.f;
	TestTrue(TEXT("combine succeeds under budget"),
		Inv->TryCombine({ Logs }, TEXT("plank"), FGuid::NewGuid(), Err));
	return true;
}

// ---------------------------------------------------------------------------
// Station Craft: deterministic first-satisfiable rule in table order.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageStationResolveTest, "Afterfall.Lineage.StationResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageStationResolveTest::RunTest(const FString& P)
{
	UInventoryComponent* Inv = MakeInventory();
	PutSlot(Inv, TEXT("res_wood_log"), 2, 1.f);
	FString Err;
	const FGuid Tx = FGuid::NewGuid();
	TestTrue(TEXT("auto-resolve combines"), Inv->ServerCombineFirstAvailable(Tx, Err));

	// plank (2x res_wood_log) is the first rule in table order.
	const FInventorySlot* Child = FindSlotByItemId(Inv, TEXT("mat_plank"));
	if (!TestNotNull(TEXT("child minted"), Child))
	{
		return false;
	}
	TestEqual(TEXT("first satisfiable rule chosen"), Child->Item.ProducedByRecipe, FName(TEXT("plank")));
	TestTrue(TEXT("auto-resolve records its transaction"),
		Child->Item.CreationTransactionId == Tx);

	UInventoryComponent* Empty = MakeInventory();
	TestFalse(TEXT("empty inventory resolves nothing"),
		Empty->ServerCombineFirstAvailable(FGuid::NewGuid(), Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("no recipe")));
	return true;
}

// ---------------------------------------------------------------------------
// Versioned save/load: full lineage round-trip; incompatible payloads reject.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageSaveRoundTripTest, "Afterfall.Lineage.SaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageSaveRoundTripTest::RunTest(const FString& P)
{
	FInventorySave Save;
	Save.OwnerId = TEXT("persistence_test_owner");

	FInventorySlot Slot0;
	Slot0.SlotIndex = 0;
	Slot0.Capacity = 99;
	Slot0.Item = FItemInstance(TEXT("mat_plank"), 5);
	Slot0.Item.Condition = 0.42f;
	Slot0.Item.CustomData = TEXT("made_by=plank@v1 tx=abc");
	Slot0.Item.ParentInstanceIds = { FGuid::NewGuid(), FGuid::NewGuid() };
	Slot0.Item.ProducedByRecipe = TEXT("plank");
	Slot0.Item.RecipeVersion = 1;
	Slot0.Item.CreationTransactionId = FGuid::NewGuid();

	FInventorySlot Slot1;
	Slot1.SlotIndex = 1;
	Slot1.bLocked = true;
	Save.Slots = { Slot0, Slot1 };

	const TArray<uint8> Bytes = UInventoryPersistence::ToBytes(Save);
	TestTrue(TEXT("payload non-empty"), Bytes.Num() > 16);

	FInventorySave Out;
	FString Err;
	TestTrue(TEXT("deserialize succeeds"), UInventoryPersistence::Deserialize(Bytes, Out, Err));
	TestEqual(TEXT("owner preserved"), Out.OwnerId, Save.OwnerId);
	TestEqual(TEXT("slot count preserved"), Out.Slots.Num(), 2);
	if (Out.Slots.Num() == 2)
	{
		const FItemInstance& A = Save.Slots[0].Item;
		const FItemInstance& B = Out.Slots[0].Item;
		TestEqual(TEXT("definition id"), B.ItemId, A.ItemId);
		TestTrue(TEXT("instance id stable"), B.InstanceId == A.InstanceId);
		TestEqual(TEXT("quantity"), B.Count, A.Count);
		TestTrue(TEXT("condition"), FMath::IsNearlyEqual(B.Condition, A.Condition, 0.0001f));
		TestEqual(TEXT("custom data"), B.CustomData, A.CustomData);
		TestEqual(TEXT("parent count"), B.ParentInstanceIds.Num(), A.ParentInstanceIds.Num());
		TestTrue(TEXT("parents stable"), B.ParentInstanceIds == A.ParentInstanceIds);
		TestEqual(TEXT("rule id"), B.ProducedByRecipe, A.ProducedByRecipe);
		TestEqual(TEXT("rule version"), B.RecipeVersion, A.RecipeVersion);
		TestTrue(TEXT("transaction id stable"), B.CreationTransactionId == A.CreationTransactionId);
		TestEqual(TEXT("capacity"), Out.Slots[0].Capacity, 99);
		TestEqual(TEXT("slot index"), Out.Slots[0].SlotIndex, 0);
		TestFalse(TEXT("slot 0 not locked"), Out.Slots[0].bLocked);
		TestTrue(TEXT("slot 1 locked flag"), Out.Slots[1].bLocked);
		TestTrue(TEXT("empty slot restored empty"), Out.Slots[1].IsEmpty());
	}

	// --- Incompatibility: newer version, bad magic, trailing bytes.
	TArray<uint8> NewerVersion = Bytes;
	if (NewerVersion.Num() >= 8)
	{
		NewerVersion[4] = 99; // SaveVersion sits right after the magic
		NewerVersion[5] = 0; NewerVersion[6] = 0; NewerVersion[7] = 0;
	}
	TestFalse(TEXT("newer save version rejected"),
		UInventoryPersistence::Deserialize(NewerVersion, Out, Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("newer than code")));

	TArray<uint8> BadMagic = Bytes;
	BadMagic[0] ^= 0xFF;
	TestFalse(TEXT("bad magic rejected"), UInventoryPersistence::Deserialize(BadMagic, Out, Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("bad magic")));

	TArray<uint8> Trailing = Bytes;
	Trailing.Add(0xAB);
	TestFalse(TEXT("trailing data rejected"), UInventoryPersistence::Deserialize(Trailing, Out, Err));
	TestTrue(TEXT("...reason"), Err.Contains(TEXT("trailing")));

	// --- Version policy (shared helper).
	bool bNeedsMigration = true;
	TestTrue(TEXT("same version loads"), UPersistenceService::IsSaveCompatible(1, 1, bNeedsMigration) && !bNeedsMigration);
	TestTrue(TEXT("older version migrates"), UPersistenceService::IsSaveCompatible(0, 1, bNeedsMigration) && bNeedsMigration);
	TestFalse(TEXT("newer version rejected"), UPersistenceService::IsSaveCompatible(2, 1, bNeedsMigration));

	// --- Real file round-trip in Saved/.
	const FString Path = FPaths::ProjectSavedDir() / TEXT("AfterfallTests") / TEXT("lineage_snapshot.bin");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("save file written"), UInventoryPersistence::SaveToFile(Save, Path));
	FInventorySave FromFile;
	TestTrue(TEXT("save file read back"), UInventoryPersistence::LoadFromFile(Path, FromFile, Err));
	TestEqual(TEXT("file owner"), FromFile.OwnerId, Save.OwnerId);
	if (FromFile.Slots.Num() == 2)
	{
		TestTrue(TEXT("file child identity stable"),
			FromFile.Slots[0].Item.InstanceId == Save.Slots[0].Item.InstanceId);
		TestEqual(TEXT("file parents stable"), FromFile.Slots[0].Item.ParentInstanceIds.Num(), 2);
	}
	IFileManager::Get().Delete(*Path);
	return true;
}

// ---------------------------------------------------------------------------
// Loot: repeated container interaction never regenerates or duplicates;
// generation is reproducible from its seed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLineageLootNoRegenerateTest, "Afterfall.Lineage.LootNoRegenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLineageLootNoRegenerateTest::RunTest(const FString& P)
{
	FLootProfile Profile;
	Profile.ProfileId = TEXT("t_prof");
	Profile.MinItems = 1;
	Profile.MaxItems = 2;
	FLootItem A; A.ItemId = TEXT("item_a"); A.MinCount = 1; A.MaxCount = 2; A.Weight = 1.f;
	FLootItem B; B.ItemId = TEXT("item_b"); B.MinCount = 1; B.MaxCount = 1; B.Weight = 1.f;
	Profile.Items = { A, B };

	ULootSystem* Loot = NewObject<ULootSystem>(GetTransientPackage());
	Loot->RegisterLootProfile(Profile);

	const FGuid Container = FGuid::NewGuid();
	const FLootInstance First = Loot->GenerateLoot(Container, TEXT("t_prof"), 123);
	TestTrue(TEXT("generated"), First.bHasBeenGenerated);
	TestTrue(TEXT("items rolled"), First.GeneratedItems.Num() >= 1);
	TestTrue(TEXT("instance tracked"), Loot->HasLootInstance(Container));

	// Reopen with a different seed: contents must NOT regenerate or change.
	const FLootInstance Reopen = Loot->GenerateLoot(Container, TEXT("t_prof"), 999);
	TestTrue(TEXT("reopen returns same instance"), Reopen.InstanceId == First.InstanceId);
	TestTrue(TEXT("reopen keeps contents"), Reopen.GeneratedItems == First.GeneratedItems);
	TestTrue(TEXT("reopen keeps counts"), Reopen.ItemCounts == First.ItemCounts);

	// Fresh system, same inputs: contents reproduce exactly (seeded stream).
	ULootSystem* Loot2 = NewObject<ULootSystem>(GetTransientPackage());
	Loot2->RegisterLootProfile(Profile);
	const FLootInstance Again = Loot2->GenerateLoot(Container, TEXT("t_prof"), 123);
	TestTrue(TEXT("same seed reproduces contents"), Again.GeneratedItems == First.GeneratedItems);
	TestTrue(TEXT("same seed reproduces counts"), Again.ItemCounts == First.ItemCounts);
	TestTrue(TEXT("identity is per-generation, not derived"),
		!(Again.InstanceId == FGuid()) && !(Again.InstanceId == First.InstanceId));

	// A different container rolls independently.
	const FLootInstance Other = Loot->GenerateLoot(FGuid::NewGuid(), TEXT("t_prof"), 123);
	TestTrue(TEXT("separate container, separate instance"), !(Other.InstanceId == First.InstanceId));

	// Deterministic roll key: stable per seed/table/container, distinct otherwise.
	TestEqual(TEXT("roll key stable"),
		ULootService::MakeRollKey(1337, 1, Container),
		ULootService::MakeRollKey(1337, 1, Container));
	TestTrue(TEXT("roll key per container"),
		ULootService::MakeRollKey(1337, 1, Container) !=
		ULootService::MakeRollKey(1337, 1, Other.ContainerId));
	TestTrue(TEXT("roll key per seed"),
		ULootService::MakeRollKey(1337, 1, Container) !=
		ULootService::MakeRollKey(42, 1, Container));
	return true;
}

#endif
