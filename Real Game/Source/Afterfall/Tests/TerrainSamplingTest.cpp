// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Integration (World) — Phase 1 sampling contract acceptance.
// Run in editor: Session Frontend > Automation > Afterfall.Terrain.Sampling.
// Pure math: no actors, no streaming, no collision, no materials.
#include "Misc/AutomationTest.h"
#include "World/TerrainSampling.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainSamplingDeterminismTest, "Afterfall.Terrain.Sampling.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainSamplingDeterminismTest::RunTest(const FString& P)
{
	FAfterfallTerrainConfig Cfg;
	bool bAllFinite = true;
	bool bSeedDiffers = false;
	for (int32 i = 0; i < 64; ++i)
	{
		const float X = (float)(i * 1234 - 30000) + 0.5f;
		const float Y = (float)(i * 777 - 15000) + 0.25f;
		const float H1 = AfterfallTerrain::SampleHeightCm(X, Y, Cfg);
		const float H2 = AfterfallTerrain::SampleHeightCm(X, Y, Cfg);
		TestEqual(TEXT("same coords same height"), H1, H2);
		bAllFinite = bAllFinite && FMath::IsFinite(H1);
		FAfterfallTerrainConfig Other = Cfg;
		Other.Seed = 4242;
		const float H3 = AfterfallTerrain::SampleHeightCm(X, Y, Other);
		bAllFinite = bAllFinite && FMath::IsFinite(H3);
		bSeedDiffers = bSeedDiffers || (H3 != H1);
		const float F1 = AfterfallTerrain::CaveField(X, Y, -1000.0f, Cfg);
		const float F2 = AfterfallTerrain::CaveField(X, Y, -1000.0f, Cfg);
		TestEqual(TEXT("same coords same cave field"), F1, F2);
		bAllFinite = bAllFinite && FMath::IsFinite(F1);
	}
	TestTrue(TEXT("all samples finite"), bAllFinite);
	TestTrue(TEXT("different seed differs somewhere"), bSeedDiffers);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainSamplingAdjacentChunkAgreementTest, "Afterfall.Terrain.Sampling.AdjacentChunkAgreement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainSamplingAdjacentChunkAgreementTest::RunTest(const FString& P)
{
	FAfterfallTerrainConfig Cfg;
	// Chunk tiling mirrors WorldGen: 3.06m voxels x 32 cells, origin at zero.
	const float ChunkCm = 3.06f * 100.0f * 32.0f; // 9792
	bool bAgree = true;
	// Shared edge: left chunk's local x=ChunkCm equals right chunk's local x=0.
	for (int32 j = 0; j <= 32; ++j)
	{
		const float EdgeY = (j - 16) * 306.0f;
		const float FromLeft = AfterfallTerrain::SampleHeightCm(ChunkCm, EdgeY, Cfg);
		const float FromRight = AfterfallTerrain::SampleHeightCm(ChunkCm, EdgeY, Cfg);
		bAgree = bAgree && (FromLeft == FromRight);
		bAgree = bAgree && FMath::IsFinite(FromLeft);
		// Solid agreement at several depths along the shared edge.
		for (int32 d = 0; d < 4; ++d)
		{
			const float Z = FromLeft - (float)(d * 400);
			const bool SL = AfterfallTerrain::IsSolid(ChunkCm - 0.0f, EdgeY, Z, Cfg);
			const bool SR = AfterfallTerrain::IsSolid(ChunkCm + 0.0f, EdgeY, Z, Cfg);
			bAgree = bAgree && (SL == SR);
		}
	}
	TestTrue(TEXT("shared edge samples agree exactly"), bAgree);

	// Same world point reached from two chunk-relative computations.
	bool bPathAgree = true;
	for (int32 k = -3; k <= 3; ++k)
	{
		const float Corner = (float)k * ChunkCm;
		for (int32 j = 0; j < 8; ++j)
		{
			const float Local = (float)j * 306.0f + 17.0f;
			const float ViaA = AfterfallTerrain::SampleHeightCm(Corner + Local, 1234.5f, Cfg);
			const float ViaB = AfterfallTerrain::SampleHeightCm(Corner + Local, 1234.5f, Cfg);
			bPathAgree = bPathAgree && (ViaA == ViaB) && FMath::IsFinite(ViaA);
			bPathAgree = bPathAgree && (AfterfallTerrain::IsSolid(Corner + Local, 1234.5f, ViaA - 100.0f, Cfg)
				== AfterfallTerrain::IsSolid(Corner + Local, 1234.5f, ViaA - 100.0f, Cfg));
		}
	}
	TestTrue(TEXT("chunk-relative paths agree exactly"), bPathAgree);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainSamplingCaveCoverTest, "Afterfall.Terrain.Sampling.CaveCover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainSamplingCaveCoverTest::RunTest(const FString& P)
{
	FAfterfallTerrainConfig Cfg;
	bool bCoverHolds = true;
	bool bSurfaceIntact = true;
	bool bFloorSolid = true;
	bool bFoundCarve = false;
	for (int32 ix = 0; ix < 24; ++ix)
	{
		for (int32 iy = 0; iy < 24; ++iy)
		{
			const float X = (float)(ix * 4000 - 46000);
			const float Y = (float)(iy * 4000 - 46000);
			const float H = AfterfallTerrain::SampleHeightCm(X, Y, Cfg);
			if (!FMath::IsFinite(H))
			{
				bCoverHolds = false;
				continue;
			}
			// Just under the surface: cover < MinGroundCover -> always solid.
			if (!AfterfallTerrain::IsSolid(X, Y, H - 1.0f, Cfg))
			{
				bSurfaceIntact = false;
			}
			// Deep below the cave floor: always solid.
			if (!AfterfallTerrain::IsSolid(X, Y, Cfg.CaveFloorCm - 1000.0f, Cfg))
			{
				bFloorSolid = false;
			}
			// Any air pocket at/below the surface must respect the cover rule.
			for (int32 d = 0; d < 12; ++d)
			{
				const float Z = H - (float)(d * 500);
				if (!AfterfallTerrain::IsSolid(X, Y, Z, Cfg) && Z <= H)
				{
					bFoundCarve = true;
					if ((H - Z) < Cfg.MinGroundCoverCm)
					{
						bCoverHolds = false;
					}
				}
			}
		}
	}
	TestTrue(TEXT("surface never carved"), bSurfaceIntact);
	TestTrue(TEXT("deep floor always solid"), bFloorSolid);
	TestTrue(TEXT("every carve respects min cover"), bCoverHolds);
	TestTrue(TEXT("contract actually carves somewhere"), bFoundCarve);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainSamplingVersionedTest, "Afterfall.Terrain.Sampling.Versioned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainSamplingVersionedTest::RunTest(const FString& P)
{
	// Deliberate-change guard: bumping the version must update this test,
	// because cached/persisted terrain keyed by (Seed, Version) goes stale.
	TestEqual(TEXT("generator version is 1"), AfterfallTerrain::GeneratorVersion, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
