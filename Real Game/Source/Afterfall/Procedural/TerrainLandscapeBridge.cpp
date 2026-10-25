// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — height conversion + layout resolver.
#include "Procedural/TerrainLandscapeBridge.h"

uint16 AfterfallTerrainLandscape::EncodeHeight(float WorldZcm, float ActorScaleZ)
{
	const float Scale = FMath::Max(ActorScaleZ, KINDA_SMALL_NUMBER);
	const float Packed = WorldZcm / Scale * HeightInvScale + HeightMid;
	return static_cast<uint16>(FMath::RoundToInt(FMath::Clamp(Packed, 0.0f, (float)HeightMax)));
}

float AfterfallTerrainLandscape::DecodeHeight(uint16 Packed, float ActorScaleZ)
{
	const float Scale = FMath::Max(ActorScaleZ, KINDA_SMALL_NUMBER);
	return (static_cast<float>(Packed) - HeightMid) / HeightInvScale * Scale;
}

bool AfterfallTerrainLandscape::ResolveLayout(float WorldSizeCm, float SampleSpacingCm,
	FAfterfallLandscapeLayout& Out, FString& OutError)
{
	Out = FAfterfallLandscapeLayout();
	if (SampleSpacingCm <= 0.0f || WorldSizeCm <= 0.0f)
	{
		OutError = TEXT("WorldSizeCm and SampleSpacingCm must be > 0");
		return false;
	}
	const int32 NeededQuads = FMath::CeilToInt(WorldSizeCm / SampleSpacingCm);
	if (NeededQuads > MaxVertsPerSide)
	{
		OutError = FString::Printf(TEXT("needs %d quads/side, over the %d engine verts limit"),
			NeededQuads, MaxVertsPerSide);
		return false;
	}
	// Allowed section sizes, largest first (prefer fewer, bigger components).
	const int32 QuadsOptions[] = { 255, 127, 63, 31, 15, 7 };
	const int32 SectionsOptions[] = { 1, 2 };
	bool bFound = false;
	int32 BestSections = TNumericLimits<int32>::Max();
	int32 BestComps = TNumericLimits<int32>::Max();
	int32 BestWaste = TNumericLimits<int32>::Max();
	for (int32 Sections : SectionsOptions)
	{
		for (int32 Quads : QuadsOptions)
		{
			const int32 CompQuads = Sections * Quads;
			const int32 Comps = FMath::DivideAndRoundUp(NeededQuads, CompQuads);
			if (Comps < 1)
			{
				continue;
			}
			const int32 Total = Comps * CompQuads;
			// Engine rule: whole components below 8191 verts, max 32 per side.
			if (Total > (MaxVertsPerSide / CompQuads) * CompQuads)
			{
				continue;
			}
			if (Comps > MaxComponentsPerSide)
			{
				continue;
			}
			// Lexicographic: 1-section components first (finest culling /
			// collision / streaming granularity, most standard layout),
			// then fewest components, then least wasted coverage.
			// E.g. 2000 m at 50 cm -> 16x16 of 255 quads (1x1 sections).
			const int32 Waste = Total - NeededQuads;
			if (Sections < BestSections
				|| (Sections == BestSections && Comps < BestComps)
				|| (Sections == BestSections && Comps == BestComps && Waste < BestWaste))
			{
				BestSections = Sections;
				BestComps = Comps;
				BestWaste = Waste;
				Out.QuadsPerSection = Quads;
				Out.SectionsPerComponent = Sections;
				Out.ComponentsX = Comps;
				Out.ComponentsY = Comps;
				Out.TotalQuadsX = Total;
				Out.TotalQuadsY = Total;
				bFound = true;
			}
		}
	}
	if (!bFound)
	{
		OutError = TEXT("no valid section/component combination found");
		return false;
	}
	Out.CoveredCmX = Out.TotalQuadsX * SampleSpacingCm;
	Out.CoveredCmY = Out.TotalQuadsY * SampleSpacingCm;
	return true;
}
