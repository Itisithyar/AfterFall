// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — Afterfall heightfield <-> Unreal Landscape bridge.
//
// SCOPE: pure conversion + layout math ONLY. No Landscape actor is created,
// no scene/streaming/networking/material file is touched. This keeps the
// integration small and reversible: it proves generated height samples turn
// into valid Landscape height data before any .umap work happens.
//
// ENCODING (verified against UE 5.8
// Engine/Source/Runtime/Landscape/Public/LandscapeDataAccess.h):
//   LocalZ = (U16 - 32768) / 128            (LANDSCAPE_ZSCALE = 1/128)
//   WorldZ = LocalZ * ActorScaleZ
//   U16 = clamp(round(WorldZ / ActorScaleZ * 128 + 32768), 0, 65535)
// Landscape samples sit on a 100 cm local grid; world spacing =
// 100 cm * ActorScaleXY. So 50 cm samples need ActorScaleXY = 0.5, and
// ActorScaleZ = 100 gives a +/-256 m height range with 0.78 cm quanta.
//
// LAYOUT (Unreal constraints): quads per section in {7,15,31,63,127,255},
// sections per component in {1,2} (1x1 or 2x2). Overall size must fit the
// engine's 8192 limit. ComponentSizeQuads = SectionsPerComponent *
// QuadsPerSection. This file only computes/validates a layout; it never
// creates components.
#pragma once

#include "CoreMinimal.h"
#include "TerrainLandscapeBridge.generated.h"

/** A validated Landscape configuration for a world size + sample spacing. */
USTRUCT(BlueprintType)
struct FAfterfallLandscapeLayout
{
	GENERATED_BODY()

	/** Quads per section edge (7/15/31/63/127/255). */
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") int32 QuadsPerSection = 255;
	/** Sections per component edge (1 or 2). */
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") int32 SectionsPerComponent = 1;
	/** Component count per side. */
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") int32 ComponentsX = 0;
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") int32 ComponentsY = 0;
	/** Total quads per side actually allocated (>= requested). */
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") int32 TotalQuadsX = 0;
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") int32 TotalQuadsY = 0;
	/** World size covered, in cm. */
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") float CoveredCmX = 0.0f;
	UPROPERTY(VisibleAnywhere, Category = "TerrainSim|Landscape") float CoveredCmY = 0.0f;

	int32 ComponentSizeQuads() const { return SectionsPerComponent * QuadsPerSection; }
};

namespace AfterfallTerrainLandscape
{
	/** Engine-verified constants (see file header). */
	constexpr float HeightMid = 32768.0f;
	constexpr float HeightInvScale = 128.0f;
	constexpr int32 HeightMax = 65535;
	/** Engine size rule (NewLandscape customization, UE 5.8): whole components
	 *  below 8191 verts, and at most 32 components per side. Max quads for a
	 *  component size C is (8191 / C) * C. */
	constexpr int32 MaxVertsPerSide = 8191;
	constexpr int32 MaxComponentsPerSide = 32;

	/** World Z (cm) -> Landscape uint16 at the given actor Z scale. */
	uint16 EncodeHeight(float WorldZcm, float ActorScaleZ);
	/** Landscape uint16 -> world Z (cm) at the given actor Z scale. */
	float DecodeHeight(uint16 Packed, float ActorScaleZ);

	/** World X/Y (cm, origin-centered) -> Landscape sample index. */
	inline int32 WorldToSample(float WorldCm, float SampleSpacingCm, int32 TotalQuads)
	{
		return FMath::RoundToInt(WorldCm / SampleSpacingCm + TotalQuads * 0.5f);
	}
	/** Landscape sample index -> world X/Y (cm, origin-centered). */
	inline float SampleToWorld(int32 Sample, float SampleSpacingCm, int32 TotalQuads)
	{
		return (Sample - TotalQuads * 0.5f) * SampleSpacingCm;
	}

	/**
	 * Resolve the valid layout covering WorldSizeCm at SampleSpacingCm.
	 * Prefers 1-section components (granularity), then the fewest
	 * components, then least waste: e.g. 16x16 of 255 quads for
	 * 2000 m at 50 cm.
	 * Returns false with a reason when impossible (e.g. 2000 m at 10 cm
	 * needs 20000 quads > the 8191-verts engine rule).
	 */
	bool ResolveLayout(float WorldSizeCm, float SampleSpacingCm,
		FAfterfallLandscapeLayout& Out,
		FString& OutError);
}
