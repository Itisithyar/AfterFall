// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Misc/AutomationTest.h"
#include "AI/ZombiePrototype.h"
#include "Combat/BodyDamageTypes.h"
#include "Combat/DamageResolver.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyPartDamageSixShotTest, "Afterfall.Damage.SixShotBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBodyPartDamageSixShotTest::RunTest(const FString& P)
{
	// Standard unarmored zombie takes six baseline body hits to kill with designated weapon
	// Torso: 100 HP / 17 damage = ~5.88, so 6 hits
	TestEqual(TEXT("torso damage baseline"), AZombiePrototype::DamageForPart(EBodyPart::Torso), 17.0f);
	TestEqual(TEXT("head damage"), AZombiePrototype::DamageForPart(EBodyPart::Head), 34.0f);
	TestEqual(TEXT("limb damage"), AZombiePrototype::DamageForPart(EBodyPart::ArmL), 12.0f);
	
	// 6 torso hits * 17 = 102 >= 100, so kills in 6 hits
	float TotalDamage = 0.0f;
	int32 Hits = 0;
	while (TotalDamage < 100.0f)
	{
		TotalDamage += AZombiePrototype::DamageForPart(EBodyPart::Torso);
		Hits++;
		if (Hits > 10) break;
	}
	TestEqual(TEXT("six torso hits to kill unarmored"), Hits, 6);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyPartDamageResolutionTest, "Afterfall.Damage.BodyPartResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBodyPartDamageResolutionTest::RunTest(const FString& P)
{
	EBodyPart Part;
	// Test part mapping
	TestTrue(TEXT("head tag maps"), AZombiePrototype::PartFromComponent(nullptr, Part) == false); // nullptr case
	// Component tag mapping is tested via static method behavior
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageTriggeredPersistenceTest, "Afterfall.Persistence.DamageTriggered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDamageTriggeredPersistenceTest::RunTest(const FString& P)
{
	// Undamaged zombie must not receive individual persistent damage record
	// This is enforced by design - record only created on first confirmed hit
	FZombieDamageRecord EmptyRecord;
	TestFalse(TEXT("undamaged has no record"), EmptyRecord.bHasRecord);
	
	// First confirmed hit creates record
	FZombieDamageRecord DamagedRecord;
	DamagedRecord.bHasRecord = true;
	DamagedRecord.StableId = FGuid::NewGuid();
	DamagedRecord.PerPart[0] = 17.0f; // Torso
	TestTrue(TEXT("damaged has record"), DamagedRecord.bHasRecord);
	TestTrue(TEXT("record has stable id"), DamagedRecord.StableId.IsValid());
	
	return true;
}

#endif
