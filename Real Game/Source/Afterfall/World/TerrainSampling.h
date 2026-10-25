// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Integration (World) — Phase 1 sampling contract ONLY.
//
// Hybrid heightfield-plus-caves contract for deterministic world-space
// terrain. This file defines HOW terrain is sampled; it does NOT spawn
// actors, stream chunks, cook collision, or assign materials (later phases).
//
// Rules:
//   * Every sample is a pure function of WORLD-SPACE centimeters + Seed.
//     Adjacent chunks sampling the same world coordinate get the same value
//     by construction (no shared state, no RNG, no per-chunk offsets).
//   * Solid(x,y,z) = (z <= Height(x,y)) with cave voids carved out.
//     Caves can never break the surface: carving requires at least
//     MinGroundCoverCm of terrain above AND WorldZ above CaveFloorCm.
//   * TerrainGeneratorVersion versions the algorithm. Persisted/streamed
//     edits must be keyed by (Seed, Version); a version bump means cached
//     terrain is stale. Bumping the version is deliberate: the
//     Versioned test asserts its exact value.
#pragma once

#include "CoreMinimal.h"
#include "TerrainSampling.generated.h"

/** Tunable terrain shape. All distances in centimeters (UE convention). */
USTRUCT(BlueprintType)
struct FAfterfallTerrainConfig
{
	GENERATED_BODY()

	/** World seed. Same seed + same coords = same terrain on every machine. */
	UPROPERTY(EditAnywhere, Category = "Terrain") int32 Seed = 1337;

	/** Base ground level. Broad/hill/detail layers add on top. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float BaseHeightCm = 0.0f;

	/** Broad-relief amplitude in open valley regions. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float ValleyAmpCm = 800.0f;

	/** Broad-relief amplitude in mountainous regions. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float MountainAmpCm = 4500.0f;

	/** Wavelength of the broad valley/mountain layer. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float BroadWavelengthCm = 120000.0f;

	/** Mid-scale hill amplitude / wavelength. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float HillAmpCm = 1200.0f;
	UPROPERTY(EditAnywhere, Category = "Terrain") float HillWavelengthCm = 12000.0f;

	/** Fine surface detail amplitude / wavelength. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float DetailAmpCm = 150.0f;
	UPROPERTY(EditAnywhere, Category = "Terrain") float DetailWavelengthCm = 1500.0f;

	/** Region lattice cell: one smoothed region type per cell. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float RegionCellCm = 50000.0f;

	/** Cave field threshold in [0,1]: higher = fewer/sparser caves. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float CaveThreshold = 0.60f;

	/** Cave noise wavelength. Larger = bigger tunnels/rooms. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float CaveWavelengthCm = 2500.0f;

	/** Caves never carve within this cover below the surface. */
	UPROPERTY(EditAnywhere, Category = "Terrain") float MinGroundCoverCm = 600.0f;

	/** Caves never carve below this world Z (keeps a solid floor). */
	UPROPERTY(EditAnywhere, Category = "Terrain") float CaveFloorCm = -5000.0f;
};

namespace AfterfallTerrain
{
	/** Algorithm version. Bump ONLY with a deliberate contract change. */
	constexpr int32 GeneratorVersion = 1;

	/** Deterministic 2D integer-lattice hash. Pure uint32 math: identical on
	 *  every platform. Salt separates independent noise channels. */
	uint32 Hash2(int32 X, int32 Y, int32 Seed, int32 Salt);

	/** Deterministic 3D integer-lattice hash (cave channel). */
	uint32 Hash3(int32 X, int32 Y, int32 Z, int32 Seed, int32 Salt);

	/** Hash -> [0,1). */
	inline float UnitFromHash(uint32 H) { return (H >> 8) * (1.0f / 16777216.0f); }

	/** Smooth (C1) 2D value noise over the integer lattice. */
	float ValueNoise2D(float X, float Y, int32 Seed, int32 Salt);

	/** Smooth (C1) 3D value noise over the integer lattice. */
	float ValueNoise3D(float X, float Y, float Z, int32 Seed, int32 Salt);

	/** Fractal sum of ValueNoise2D. Octaves clamped to 1..4. */
	float Fbm2(float X, float Y, int32 Seed, int32 Salt, int32 Octaves);

	/** Fractal sum of ValueNoise3D. Octaves clamped to 1..4. */
	float Fbm3(float X, float Y, float Z, int32 Seed, int32 Salt, int32 Octaves);

	/** Smoothed region control in [0,1]: 0 = open valley, 1 = mountains.
	 *  C1-continuous, so region borders blend without cliffs. */
	float RegionMountain(float WorldXcm, float WorldYcm, const FAfterfallTerrainConfig& Cfg);

	/** Ground surface height in world cm. Pure function of (X, Y). */
	float SampleHeightCm(float WorldXcm, float WorldYcm, const FAfterfallTerrainConfig& Cfg);

	/** Cave field in [0,1]. Carve where CaveField > Cfg.CaveThreshold. */
	float CaveField(float WorldXcm, float WorldYcm, float WorldZcm, const FAfterfallTerrainConfig& Cfg);

	/** True when the point is solid terrain: under the heightfield AND not
	 *  carved by a cave. Caves require MinGroundCoverCm of cover and
	 *  WorldZ >= CaveFloorCm, so the surface and the deep floor stay solid. */
	bool IsSolid(float WorldXcm, float WorldYcm, float WorldZcm, const FAfterfallTerrainConfig& Cfg);
}
