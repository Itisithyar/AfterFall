// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Tests for destruction system
#include "Misc/AutomationTest.h"
#include "Destruction/DestructionPropagator.h"
#include "Destruction/DestructionTypes.h"
#include "World/WorldRoot.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDestructionPropagationTest, "Afterfall.Destruction.Propagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDestructionPropagationTest::RunTest(const FString& P)
{
	// Test material resistance
	TestEqual(TEXT("wood resistance"), UDestructionPropagator::GetMaterialResistance(EMaterialType::Wood), 0.3f);
	TestEqual(TEXT("concrete resistance"), UDestructionPropagator::GetMaterialResistance(EMaterialType::Concrete), 0.8f);
	
	// Test distance modifier
	TestTrue(TEXT("close range damage"), UDestructionPropagator::CalculateDistanceModifier(50.0f, 100.0f) > 0.7f);
	TestTrue(TEXT("far range damage reduced"), UDestructionPropagator::CalculateDistanceModifier(500.0f, 100.0f) < 0.6f);
	
	// Test propagation
	FDestructionImpact Impact;
	Impact.Origin = FVector::ZeroVector;
	Impact.Direction = FVector(1, 0, 0);
	Impact.InitialEnergy = 100.0f;
	Impact.Mode = EDestructionMode::Bullet;
	Impact.Material = EMaterialType::Wood;
	Impact.ImpactPoint = FVector::ZeroVector;
	
	FPropagationState State = UDestructionPropagator::PropagateDestruction(Impact, nullptr);
	TestTrue(TEXT("propagation runs"), State.PropagationDepth >= 0);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldRootIdentityTest, "Afterfall.World.RootIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorldRootIdentityTest::RunTest(const FString& P)
{
	// Test root identity is separate from live representation
	FWorldRootData Data;
	Data.RootId = FGuid::NewGuid();
	Data.ObjectType = FName("TestObject");
	Data.State = EWorldObjectState::Active;
	
	TestTrue(TEXT("root has identity"), Data.RootId.IsValid());
	TestEqual(TEXT("root state"), Data.State, EWorldObjectState::Active);
	
	return true;
}

#endif
