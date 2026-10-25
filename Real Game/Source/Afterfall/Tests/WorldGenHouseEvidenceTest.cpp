// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Integration (World generation) — headless evidence probe, no
// gameplay change. Answers with numbers why the log says "N planned, 0 with
// geometry inside stream radius": runs the exact PlanHouses + InsideRadius
// path GenerateWorld uses, with the DEFAULT config, and UE_LOGs the counts
// plus per-house distance and InsideRadius result. Always passes.
#include "Misc/AutomationTest.h"
#include "Afterfall.h"
#include "World/WorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenHouseEvidenceTest, "Afterfall.WorldGen.HouseEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWorldGenHouseEvidenceTest::RunTest(const FString& P)
{
	UAfterfallWorldGenSubsystem* Sub = NewObject<UAfterfallWorldGenSubsystem>(GetTransientPackage());
	if (!Sub)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("HOUSEEVIDENCE FAIL: subsystem NewObject returned null"));
		return false;
	}
	// DEFAULT seed, radius and stream settings — the same defaults
	// GenerateWorld reads from a fresh subsystem.
	Sub->Config = FAfterfallWorldConfig();
	const FAfterfallWorldConfig& C = Sub->Config;
	TArray<FHousePlacement> Planned = Sub->PlanHouses(C);
	int32 InsideCount = 0;
	for (int32 i = 0; i < Planned.Num(); ++i)
	{
		const FHousePlacement& H = Planned[i];
		const double DistM = FVector::Dist2D(H.LocationM, C.OriginM);
		// The house-geometry gate in GenerateWorld (WorldGen.cpp):
		//   const bool bNear = InsideRadius(Config.OriginM, House.LocationM, Config.StreamRadiusM);
		const bool bNear = UAfterfallWorldGenSubsystem::InsideRadius(C.OriginM, H.LocationM, C.StreamRadiusM);
		if (bNear)
		{
			++InsideCount;
		}
		UE_LOG(LogAfterfall, Log,
			TEXT("HOUSEEVIDENCE house idx=%d distM=%.1f bNear=%d origin=(%.1f,%.1f) loc=(%.1f,%.1f) streamR=%.1f footprint=%.1f"),
			i, DistM, bNear ? 1 : 0,
			C.OriginM.X, C.OriginM.Y, H.LocationM.X, H.LocationM.Y,
			C.StreamRadiusM, H.Blueprint.FootprintM);
	}
	// GenerateWorld increments HouseGeometry exactly when bNear is true, so
	// withGeometry == insideRadius by construction of the condition.
	const int32 WithGeometry = InsideCount;
	UE_LOG(LogAfterfall, Log,
		TEXT("HOUSEEVIDENCE seed=%d streamRadius=%.1f planned=%d insideRadius=%d withGeometry=%d"),
		C.Seed, C.StreamRadiusM, Planned.Num(), InsideCount, WithGeometry);
	return true;
}

#endif
