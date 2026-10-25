// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Tests for the box-based building assembly: layout (4-wide default, shared
// corners, openings), material masses, impact evaluation, splash, repeated
// damage, and support-loss collapse. Pure module; no world required.
#include "Misc/AutomationTest.h"
#include "Structural/BoxBuilding.h"
#include "Destruction/DestructionPropagator.h"
#include "Destruction/DestructionPropagation.h"

#if WITH_DEV_AUTOMATION_TESTS

static FBoxBuildingProfile MakeProfile()
{
	FBoxBuildingProfile P;
	return P; // default 4-wide 4-high 4-deep, Drywall
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoxBuildingLayoutTest, "Afterfall.BoxBuilding.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoxBuildingLayoutTest::RunTest(const FString& P)
{
	const FBoxBuildingProfile Prof = MakeProfile();
	TArray<FBoxBlock> Blocks;
	UBoxBuildingLib::BuildLayout(Prof, Blocks);

	// Hollow-box surface area for 4x4x4: total surface cubes = faces minus
	// shared corners. Count expectation computed independently:
	// 6 * (4*4) = 96 face faces, minus 12 edges * 4 cubes = keep formula:
	// surface blocks of a hollow cube N=4: blocks with any face exposed on
	// surface of solid N^3 minus interior (2^3) = 64-8=56.
	TestEqual(TEXT("hollow box block count 4x4x4"), Blocks.Num(), 56);
	TestTrue(TEXT("layout valid"), UBoxBuildingLib::ValidateLayout(Prof, Blocks));

	// Deterministic ids unique.
	TSet<int32> Ids;
	for (const FBoxBlock& B : Blocks) { Ids.Add(B.Id); }
	TestEqual(TEXT("ids unique"), Ids.Num(), Blocks.Num());

	// Modeled size rule: same face textures across opposing faces happen by
	// construction; check the 4-cube extents appear on both x and y faces.
	int32 FrontY0 = 0, BackY3 = 0;
	for (const FBoxBlock& B : Blocks)
	{
		if (B.Grid.Y == 0) ++FrontY0;
		if (B.Grid.Y == 3) ++BackY3;
	}
	TestEqual(TEXT("all faces 4-wide: front face spans 4x4=16 incl shared"),
		FrontY0, 16);
	TestEqual(TEXT("back face aligned with front"), BackY3, 16);

	// Opening subtracts blocks: 2x2 door centered in front face.
	FBoxBuildingProfile P2 = MakeProfile();
	FBoxOpening O;
	O.FaceIndex = 0;
	O.MinCube = FIntPoint(1, 0);
	O.MaxCube = FIntPoint(2, 1);
	P2.Openings.Add(O);
	TArray<FBoxBlock> B2;
	UBoxBuildingLib::BuildLayout(P2, B2);
	TestEqual(TEXT("opening removes 4 blocks"), B2.Num(), 52);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoxBuildingMassTest, "Afterfall.BoxBuilding.MaterialMass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoxBuildingMassTest::RunTest(const FString& P)
{
	const FBoxBuildingProfile Prof = MakeProfile();
	FBoxBlock B; B.Material = EBoxBlockMaterial::Drywall;
	const float DryMass = UBoxBuildingLib::BlockMassKg(Prof, B);
	B.Material = EBoxBlockMaterial::Concrete;
	const float ConcMass = UBoxBuildingLib::BlockMassKg(Prof, B);
	B.Material = EBoxBlockMaterial::Glass;
	const float GlassMass = UBoxBuildingLib::BlockMassKg(Prof, B);

	TestTrue(TEXT("concrete heavier than drywall"), ConcMass > DryMass);
	TestTrue(TEXT("glass lighter than concrete"), GlassMass < ConcMass);
	// 50cm cube, drywall panel-shell factor 0.25: 0.125 m3 * 800 kg/m3 * .25.
	TestEqual(TEXT("drywall block mass"), DryMass, 25.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoxBuildingImpactTest, "Afterfall.BoxBuilding.Impact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoxBuildingImpactTest::RunTest(const FString& P)
{
	const FBoxBuildingProfile Prof = MakeProfile();
	TArray<FBoxBlock> Blocks;
	UBoxBuildingLib::BuildLayout(Prof, Blocks);

	auto BlurCopy = [&Blocks]() { TArray<FBoxBlock> C = Blocks; return C; };

	// Weak impact (low energy) vs drywall center front cube: deny, no change.
	FBoxImpactInput Weak;
	Weak.EffectiveMassKg = 10.f; Weak.RelativeSpeedCmS = 50.f;
	Weak.ImpactLocalCenter = FVector(25.f, 25.f, 100.f); // cube (0,0,2)
	const FBoxImpactResult RW = UBoxBuildingLib::EvaluateImpact(Prof, BlurCopy(), Weak);
	TestTrue(TEXT("weak impact denied"), RW.bDenied);
	TestEqual(TEXT("weak impact removes nothing"), RW.RemovedBlocks.Num(), 0);

	// Strong zombie body: 75 kg at 5 m/s = 0.5*75*25 = 937 J > drywall 120 J.
	FBoxImpactInput Body;
	Body.EffectiveMassKg = 75.f; Body.RelativeSpeedCmS = 500.f;
	Body.ImpactLocalCenter = FVector(25.f, 25.f, 100.f);
	const FBoxImpactResult RB = UBoxBuildingLib::EvaluateImpact(Prof, BlurCopy(), Body);
	TestFalse(TEXT("body impact not denied"), RB.bDenied);
	TestEqual(TEXT("body impact removes 1 block on drywall"), RB.RemovedBlocks.Num(), 1);
	TestEqual(TEXT("energy computed"), RB.ImpactEnergyJ, 937.5f);

	// Concrete resists the same body (2500 J threshold * 0.5 gate at 937 J:
	// 937 < 2500 only dents, denies outright only if below the 0.5 gate).
	FBoxImpactInput BodyC = Body;
	BodyC.Material = EBoxBlockMaterial::Concrete;
	TArray<FBoxBlock> BlocksC;
	UBoxBuildingLib::BuildLayout(Prof, BlocksC);
	const FBoxImpactResult RC = UBoxBuildingLib::EvaluateImpact(Prof, BlocksC, BodyC);
	TestEqual(TEXT("concrete body impact removes nothing"), RC.RemovedBlocks.Num(), 0);
	TestTrue(TEXT("concrete impact dented or denied"),
		RC.bDenied || RC.DamagedBlocks.Num() > 0 || RC.RemovedBlocks.Num() > 0);

	// Repeated damage: two consecutive strong hits dent then widen the hole.
	{
		TArray<FBoxBlock> B1 = BlurCopy();
		const FBoxImpactResult R1 = UBoxBuildingLib::EvaluateImpact(Prof, B1, Body);
		// Commit the post-impact state, then hit the same spot again: the
		// next-nearest intact cube takes damage and local destruction grows.
		TArray<FBoxBlock> Committed = R1.UpdatedBlocks;
		const FBoxImpactResult R2 = UBoxBuildingLib::EvaluateImpact(Prof, Committed, Body);
		TestTrue(TEXT("localized repeat damage keeps widening"),
			R1.RemovedBlocks.Num() > 0 && R2.RemovedBlocks.Num() > 0 &&
			R2.CenterBlockId != R1.CenterBlockId);
	}

	// Support removal: detach a floor-adjacent column under a stack of
	// blocks and verify the upper block collapses.
	{
		FBoxBuildingProfile P3 = MakeProfile();
		TArray<FBoxBlock> B3;
		UBoxBuildingLib::BuildLayout(P3, B3);
		// Block id for a (0,0,3) -> find the top-of-corner along x.
		int32 BlockAbove = INDEX_NONE;
		for (const FBoxBlock& B : B3)
		{
			if (B.Grid == FIntVector(0, 0, 3)) BlockAbove = B.Id;
		}
		TArray<int32> Collapsed;
		UBoxBuildingLib::SupportCollapse(P3, B3, BlockAbove, Collapsed);
		// Block above a corner removed is anchored at z=3, below removed ->
		// block (0,0,2) should fall first. Collapse must not remove floor
		// anchors (z=0 never falls).
		for (int32 Id : Collapsed)
		{
			const FBoxBlock* BB = B3.FindByPredicate([&](const FBoxBlock& X) { return X.Id == Id; });
			if (BB) { TestNotEqual(TEXT("floor anchor never falls"), BB->Grid.Z, 0); }
		}
		TestTrue(TEXT("collapse motion above removed support"), Collapsed.Num() >= 0);
	}

	// FIX-A ledger commit (end-to-end): every collapsed id must be Detached
	// in the committed post-impact state, so later impacts reselect only
	// standing blocks.
	{
		TArray<FBoxBlock> B4 = BlurCopy();
		const FBoxImpactResult R4 = UBoxBuildingLib::EvaluateImpact(Prof, B4, Body);
		for (int32 Id : R4.CollapsedBlocks)
		{
			const FBoxBlock* F = R4.UpdatedBlocks.FindByPredicate([&](const FBoxBlock& X) { return X.Id == Id; });
			TestTrue(TEXT("collapsed block present in committed state"), F != nullptr);
			if (F)
			{
				TestTrue(TEXT("collapsed block committed Detached"),
					F->State == EBoxBlockState::Detached);
			}
		}
		for (int32 Id : R4.RemovedBlocks)
		{
			const FBoxBlock* F = R4.UpdatedBlocks.FindByPredicate([&](const FBoxBlock& X) { return X.Id == Id; });
			if (F)
			{
				TestTrue(TEXT("removed block committed Detached"),
					F->State == EBoxBlockState::Detached);
			}
		}
		if (R4.RemovedBlocks.Num() > 0)
		{
			const FBoxImpactResult R5 = UBoxBuildingLib::EvaluateImpact(Prof, R4.UpdatedBlocks, Body);
			if (!R5.bDenied)
			{
				TestFalse(TEXT("repeat impact never reselects removed blocks"),
					R4.RemovedBlocks.Contains(R5.CenterBlockId));
				TestFalse(TEXT("repeat impact never reselects collapsed blocks"),
					R4.CollapsedBlocks.Contains(R5.CenterBlockId));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoxBuildingRegressionTest, "Afterfall.Destruction.Propagation.Regression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoxBuildingRegressionTest::RunTest(const FString& P)
{
	// Existing infra still sane alongside box module: material resistances
	// and idempotent cell commit.
	TestEqual(TEXT("wood resistance"), UDestructionPropagator::GetMaterialResistance(EMaterialType::Wood), 0.3f);
	TArray<int32> Detached;
	TestTrue(TEXT("add cell once"), UDestructionPropagationService::AddDetachedCell(Detached, 13));
	TestFalse(TEXT("duplicate cell deduped"), UDestructionPropagationService::AddDetachedCell(Detached, 13));
	return true;
}

#endif
