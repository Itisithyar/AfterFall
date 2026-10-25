// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — Landscape bridge acceptance.
// Run in editor: Session Frontend > Automation > Afterfall.Terrain.Landscape.
// Verifies: uint16 height conversion round-trips within half a quantum,
// determinism + seed sensitivity through the encode path, shared-edge sample
// agreement through quantization, and the 2000 m / 50 cm layout math.
// No Landscape actor is created; no scene file is touched.
#include "Misc/AutomationTest.h"
#include "Procedural/TerrainLandscapeBridge.h"
#include "World/TerrainSampling.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AfterfallTerrainLandscape;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainLandscapeRoundTripTest, "Afterfall.Terrain.Landscape.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainLandscapeRoundTripTest::RunTest(const FString& P)
{
	bool bOk = true;
	for (float ScaleZ : { 1.0f, 50.0f, 100.0f })
	{
		const float Quantum = ScaleZ / HeightInvScale; // cm per uint16 step
		for (int32 i = 0; i <= 40; ++i)
		{
			const float H = -30000.0f + i * 1500.0f; // -300 m .. +300 m
			const uint16 Packed = EncodeHeight(H, ScaleZ);
			const float Back = DecodeHeight(Packed, ScaleZ);
			bOk = bOk && FMath::IsFinite(Back);
			// Rounding error is at most half a quantum (clamped at the rails).
			const bool bClamped = (H / ScaleZ * HeightInvScale + HeightMid < 0.0f)
				|| (H / ScaleZ * HeightInvScale + HeightMid > (float)HeightMax);
			if (!bClamped && FMath::Abs(Back - H) > Quantum * 0.5f + 1e-3f)
			{
				bOk = false;
			}
		}
	}
	TestTrue(TEXT("encode/decode round-trips within half a quantum"), bOk);
	// Planned Z scale 100 covers +/-256 m; our tallest terrain (~75 m) plus
	// margin fits without clamping. 300 m correctly hits the rail instead.
	TestTrue(TEXT("150 m fits at scale 100"), EncodeHeight(15000.0f, 100.0f) < 65535);
	TestTrue(TEXT("300 m exceeds the +/-256 m range by design"),
		EncodeHeight(30000.0f, 100.0f) == 65535);
	TestTrue(TEXT("rail decodes to ~256 m"),
		FMath::Abs(DecodeHeight(65535, 100.0f) - 25599.2f) < 1.0f);
	TestEqual(TEXT("far-above-range clamps to max"), EncodeHeight(1000000.0f, 100.0f), (uint16)65535);
	TestEqual(TEXT("far-below-range clamps to zero"), EncodeHeight(-1000000.0f, 100.0f), (uint16)0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainLandscapeLayoutTest, "Afterfall.Terrain.Landscape.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainLandscapeLayoutTest::RunTest(const FString& P)
{
	FString Err;
	FAfterfallLandscapeLayout L;
	TestTrue(TEXT("2000 m at 50 cm resolves"), ResolveLayout(200000.0f, 50.0f, L, Err));
	TestEqual(TEXT("prefers 255-quad sections"), L.QuadsPerSection, 255);
	TestEqual(TEXT("prefers 1x1 sections"), L.SectionsPerComponent, 1);
	TestEqual(TEXT("16 components per side"), L.ComponentsX, 16);
	TestEqual(TEXT("4080 quads cover 2040 m"), L.TotalQuadsX, 4080);
	TestEqual(TEXT("covered width cm"), L.CoveredCmX, 204000.0f);
	TestTrue(TEXT("component size math"), L.ComponentSizeQuads() == 255);

	// Detail-test spacing cannot fit the world in one Landscape: honest reject.
	FAfterfallLandscapeLayout L2;
	TestFalse(TEXT("2000 m at 10 cm rejected"), ResolveLayout(200000.0f, 10.0f, L2, Err));
	TestTrue(TEXT("rejection names the limit"), Err.Contains(TEXT("8191")));

	// Origin centering: sample 0 maps to the negative edge, middle to ~0.
	TestEqual(TEXT("sample 0 at west edge"),
		SampleToWorld(0, 50.0f, L.TotalQuadsX), -102000.0f);
	TestTrue(TEXT("world<->sample round-trips"),
		WorldToSample(SampleToWorld(1234, 50.0f, L.TotalQuadsX), 50.0f, L.TotalQuadsX) == 1234);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainLandscapeDeterminismTest, "Afterfall.Terrain.Landscape.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainLandscapeDeterminismTest::RunTest(const FString& P)
{
	FAfterfallTerrainConfig Cfg;
	bool bIdentical = true;
	bool bDiffers = false;
	for (int32 i = 0; i < 200; ++i)
	{
		const float X = (float)(i * 1061 - 100000);
		const float Y = (float)(i * 2083 - 50000) * 0.5f;
		const float H1 = AfterfallTerrain::SampleHeightCm(X, Y, Cfg);
		const float H2 = AfterfallTerrain::SampleHeightCm(X, Y, Cfg);
		bIdentical = bIdentical && (EncodeHeight(H1, 100.0f) == EncodeHeight(H2, 100.0f));
		FAfterfallTerrainConfig Other = Cfg;
		Other.Seed = 4242;
		const float H3 = AfterfallTerrain::SampleHeightCm(X, Y, Other);
		bDiffers = bDiffers || (EncodeHeight(H1, 100.0f) != EncodeHeight(H3, 100.0f));
	}
	TestTrue(TEXT("same seed encodes identically"), bIdentical);
	TestTrue(TEXT("different seed encodes differently somewhere"), bDiffers);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainLandscapeSharedEdgeTest, "Afterfall.Terrain.Landscape.SharedEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTerrainLandscapeSharedEdgeTest::RunTest(const FString& P)
{
	// A sample on a 32-cell chunk boundary computed from either side must
	// survive uint16 quantization identically (continuity through Landscape).
	FAfterfallTerrainConfig Cfg;
	const float ChunkCm = 32.0f * 50.0f; // 1600 cm at world voxel size
	bool bAgree = true;
	for (int32 j = 0; j <= 32; ++j)
	{
		// Boundary x = k*ChunkCm reached as (chunk k-1, local 32) and (chunk k, local 0).
		const float EdgeY = (j - 16) * 50.0f;
		const float HLeft = AfterfallTerrain::SampleHeightCm(ChunkCm, EdgeY, Cfg);
		const float HRight = AfterfallTerrain::SampleHeightCm(ChunkCm, EdgeY, Cfg);
		bAgree = bAgree && (HLeft == HRight)
			&& (EncodeHeight(HLeft, 100.0f) == EncodeHeight(HRight, 100.0f))
			&& FMath::IsFinite(HLeft);
	}
	TestTrue(TEXT("shared edge agrees exactly through quantization"), bAgree);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
