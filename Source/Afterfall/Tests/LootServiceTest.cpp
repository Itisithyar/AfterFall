// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Tests) — mirrors Scripts/validate_lootservice.py.
#include "Misc/AutomationTest.h"
#include "Loot/LootService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLootEntryCountTest, "Afterfall.Loot.EntryCount",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLootEntryCountTest::RunTest(const FString& P)
{
	TestEqual(TEXT("min edge"), ULootService::PickEntryCount(1, 3, 0.f), 1);
	TestEqual(TEXT("max edge"), ULootService::PickEntryCount(1, 3, 0.999f), 3);
	TestEqual(TEXT("fixed"), ULootService::PickEntryCount(2, 2, 0.5f), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLootRarityTest, "Afterfall.Loot.Rarity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLootRarityTest::RunTest(const FString& P)
{
	TMap<FName, float> G; G.Add(TEXT("Common"), 70.f); G.Add(TEXT("Uncommon"), 25.f); G.Add(TEXT("Rare"), 4.5f); G.Add(TEXT("VeryRare"), 0.5f);
	TestEqual(TEXT("common band"), ULootService::PickRarity(G, 0.1f), FName(TEXT("Common")));
	TestEqual(TEXT("uncommon band"), ULootService::PickRarity(G, 0.8f), FName(TEXT("Uncommon")));
	TestEqual(TEXT("rare band"), ULootService::PickRarity(G, 0.97f), FName(TEXT("Rare")));
	TestEqual(TEXT("vr band"), ULootService::PickRarity(G, 0.999f), FName(TEXT("VeryRare")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLootUniqueTest, "Afterfall.Loot.SingleCopy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLootUniqueTest::RunTest(const FString& P)
{
	TestTrue(TEXT("first copy mints"), ULootService::CanMintUnique(TEXT("wp_pistol_9mm"), 0, 1));
	TestFalse(TEXT("second copy blocked"), ULootService::CanMintUnique(TEXT("wp_pistol_9mm"), 1, 1));
	TestFalse(TEXT("zero cap never mints"), ULootService::CanMintUnique(TEXT("x"), 0, 0));
	int32 Idx = -99;
	TArray<FName> Items = { TEXT("a"), TEXT("b") };
	ULootService::ApplyGuarantee(Items, { TEXT("c") }, 1.f, 0.f, Idx);
	TestEqual(TEXT("guarantee replaces last"), Idx, 1);
	ULootService::ApplyGuarantee(Items, { TEXT("a") }, 1.f, 0.f, Idx);
	TestEqual(TEXT("satisfied = -1"), Idx, -1);
	return true;
}

#endif
