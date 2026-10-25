// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — seed-driven world terrain simulation contract.
//
// The user-supplied Afterfall equations are the intended model and are
// implemented AS WRITTEN (see Stage A / Stage B below). Anything derived by
// the implementer (regional H0 shaping formulas, Stage B coupling, constant
// defaults) is flagged DERIVED-AWAITING-APPROVAL in Docs/TerrainSim.md.
//
// CONVENTIONS (full detail in Docs/TerrainSim.md):
//   * Units are Unreal centimeters. VoxelSize is configurable (50 world, 10
//     detail). ChunkWidth = 32 * VoxelSize (1600 cm at 50).
//   * 33 retained samples per horizontal axis (indices 0..32) = 32 cells.
//     Extraction region is 32x32x32 CELLS using 33^3 corner samples. Halo is
//     simulation support only, never emitted.
//   * worldPos = (chunkCoord * 32 + localIndex) * VoxelSize, from GLOBAL
//     integer sample coordinates. Noise, sources, and height queries never
//     use chunk-relative floats.
//   * Density: F < 0 solid, F > 0 air, surface at F = 0. F = (z - H) + Carve.
//     Carve >= 0 only adds air. Hard rule: F is air above H + Margin,
//     enforced AFTER carving and deformation.
//   * Lap4 = 4-neighbor, Lap6 = 6-neighbor, Grad = central difference, all in
//     GRID units (per-sample); VoxelSize enters only when converting height
//     differences to world units. Boundaries clamp-to-edge (Neumann).
//   * Chunk (cx,cy,cz) owns corner samples [cx*32, cx*32+32] on each axis.
//     A neighbor's corner 0 IS my corner 32 (same global coords -> same F ->
//     bit-identical boundary vertices). Each chunk emits all its 32^3 cells;
//     coincident boundary vertices are geometrically identical, never
//     duplicate faces.
//   * ChunkWidth (1600 cm at VoxelSize 50) is independent of the 50 cm
//     structural grid and the ~300 cm foundation footprint (see
//     StructuralAlignment constants below); the building system is untouched.
#pragma once

#include "CoreMinimal.h"
#include "TerrainSim.generated.h"

/** Alignment constants for LATER construction work. Read-only here. */
namespace AfterfallTerrainAlign
{
	constexpr float StructuralGridCm = 50.0f;
	constexpr float FoundationFootprintCm = 300.0f;
}

/** One world = one config. Server picks it; clients regenerate from it. */
USTRUCT(BlueprintType)
struct FTerrainWorldConfig
{
	GENERATED_BODY()

	/** World seed. Different seed = different map. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|World") int32 Seed = 1337;
	/** World extent in meters, centered on the origin. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|World", meta = (ClampMin = "100", ClampMax = "10000")) float WorldSizeMeters = 2000.0f;
	/** Voxel edge in cm. 50 = world scale, 10 = detail tests. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|World", meta = (ClampMin = "1", ClampMax = "500")) float VoxelSize = 50.0f;
	/** Bumped whenever the world-generation meaning changes. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|World") int32 ConfigVersion = 1;

	float ChunkWidthCm() const { return 32.0f * VoxelSize; }
	float WorldHalfCm() const { return WorldSizeMeters * 50.0f; }
	/** Chunk-column count = ceil(WorldSize / ChunkWidth)^2. */
	int32 ChunkColumnCount() const;
	bool Validate(FString& OutError) const;
};

/** Region shaping presets. DERIVED DEFAULTS — awaiting owner approval. */
USTRUCT(BlueprintType)
struct FTerrainRegionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions", meta = (ClampMin = "1000")) float RegionSizeM = 256.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions", meta = (ClampMin = "0")) float TransitionBandM = 64.0f;
	/** Upper hash bounds selecting Plain / Valley / Ridge (else Plateau). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float PlainMax = 0.30f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float ValleyMax = 0.55f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float RidgeMax = 0.80f;
	/** Per-type base height (cm) and amplitude (cm). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float PlainBase = 0.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float PlainAmp = 800.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float ValleyBase = -1500.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float ValleyAmp = 1200.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float ValleyDepth = 2500.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float RidgeBase = 1000.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float RidgeAmp = 4500.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float PlateauBase = 2000.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float PlateauAmp = 1000.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions") float PlateauCap = 3500.0f;
	/** Fbm octaves / lacunarity / gain for H0 (fixed per contract). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Regions", meta = (ClampMin = "1", ClampMax = "8")) int32 H0Octaves = 5;
};

/** Stage A (2D heightfield + coupled psi) configuration. */
USTRUCT(BlueprintType)
struct FTerrainStageAConfig
{
	GENERATED_BODY()

	/** Master toggle. False => H = H0 (regional base only). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA") bool bApplyStageA = true;
	/** H' = H + Alpha*T - Beta*Lap4(H) + Eta*HashNoise + Gamma*Delta*dot(Grad(psi),Grad(H)) */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA") float Alpha = 0.5f;
	/** INPUT CHECK ONLY: Beta <= 0.25 (normalized Lap4). Not a stability proof. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0", ClampMax = "0.25")) float Beta = 0.05f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0")) float Eta = 12.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA") float Gamma = 1.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA") float Delta = 1.0f;
	/** psi' = psi + Eps*Lap4(psi) - Zeta*H*psi */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0")) float Eps = 0.08f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0")) float Zeta = 0.0001f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0", ClampMax = "64")) int32 IterationsA = 8;
	/** Halo layers each side. MUST be >= IterationsA (validated). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0", ClampMax = "32")) int32 HaloA = 8;
	/** Source-point strength (cm) and radius (samples). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "0")) float SourceStrength = 200.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA", meta = (ClampMin = "1", ClampMax = "64")) float SourceRadius = 12.0f;
	/** Literal spec signs: LapSignH=-1 (anti-diffusive), LapSignPsi=+1. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA|Signs") float LapSignH = -1.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageA|Signs") float LapSignPsi = 1.0f;

	bool Validate(FString& OutError) const;
};

/** Stage B (3D cave field + coupled psiR) configuration. PROVISIONAL coupling. */
USTRUCT(BlueprintType)
struct FTerrainStageBConfig
{
	GENERATED_BODY()

	/** R' = R + AlphaR*T_R - BetaR*Lap6(R) + EtaR*HashNoiseR + CouplingR */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB") float AlphaR = 0.4f;
	/** INPUT CHECK ONLY: BetaR <= 1/6 (normalized Lap6). Not a stability proof. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0", ClampMax = "0.1667")) float BetaR = 0.03f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float EtaR = 0.25f;
	/** PROVISIONAL (derived): CouplingR = GammaR*DeltaR*dot(Grad(psiR),Grad(R)) */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB") float GammaR = 1.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB") float DeltaR = 1.0f;
	/** psiR' = psiR + EpsR*Lap6(psiR) - ZetaR*R*psiR */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float EpsR = 0.05f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float ZetaR = 0.02f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0", ClampMax = "32")) int32 IterationsB = 8;
	/** Halo in x, y AND z. MUST be >= IterationsB (validated). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0", ClampMax = "16")) int32 HaloB = 8;
	/** Carve = smoothstep(R0, R1, R) * CarveStrength. Must exceed MinDepth scale. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB") float R0 = 0.45f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB") float R1 = 0.65f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float CarveStrength = 1200.0f;
	/** Active band: z in [H - MaxCaveDepth, H + Margin]. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float MaxCaveDepth = 3000.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float Margin = 500.0f;
	/** Depth gate: no carving above H - MinDepth except near entrances. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float MinDepth = 800.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "0")) float EntranceRadius = 1500.0f;
	/** Literal spec signs (same convention as Stage A). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB|Signs") float LapSignR = -1.0f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB|Signs") float LapSignPsiR = 1.0f;
	/** Source strength/radius for T_R (same lattice scheme as Stage A). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB") float SourceStrengthR = 0.6f;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|StageB", meta = (ClampMin = "1", ClampMax = "64")) float SourceRadiusR = 10.0f;

	bool Validate(FString& OutError) const;
};

/** Authoritative corridor input. Server-owned; versioned; hashed. */
USTRUCT(BlueprintType)
struct FTerrainCorridorInput
{
	GENERATED_BODY()

	/** Polyline in world cm. Empty = no corridor. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Corridor") TArray<FVector> Points;
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Corridor", meta = (ClampMin = "0")) float Radius = 2000.0f;
	/** Eta and high-frequency terms scale by (1 - CorridorWeight * Smoothing). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Corridor", meta = (ClampMin = "0", ClampMax = "1")) float Smoothing = 0.85f;
	/** Dips below the corridor reference floor clamp to this depth (cm). */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Corridor", meta = (ClampMin = "0")) float MaxWalkableDip = 300.0f;
	/** Monotonic revision. Part of the chunk hash; stale jobs are discarded. */
	UPROPERTY(EditAnywhere, Category = "TerrainSim|Corridor") uint32 Version = 1;

	bool Validate(FString& OutError) const;
};

namespace AfterfallTerrainSim
{
	/** Simulation-format version (hash input #1). Bump on any equation change. */
	constexpr int32 SimFormatVersion = 1;
	/** Discrete-operator version: Lap4/Lap6/Grad/HashNoise definitions (#8). */
	constexpr int32 OperatorVersion = 1;

	/** Deterministic integer hash. Pure uint32 math, platform-independent. */
	uint32 Hash2i(int32 X, int32 Y, int32 Seed, int32 Salt);
	uint32 Hash3i(int32 X, int32 Y, int32 Z, int32 Seed, int32 Salt);
	inline float Unit01(uint32 H) { return (H >> 8) * (1.0f / 16777216.0f); }
	inline float Signed11(uint32 H) { return Unit01(H) * 2.0f - 1.0f; }

	float ValueNoise2(float X, float Y, int32 Seed, int32 Salt);
	float ValueNoise3(float X, float Y, float Z, int32 Seed, int32 Salt);

	/** Region type selected by hash(regionX, regionY, Seed). */
	enum class ERegionType : uint8 { Plain, Valley, Ridge, Plateau };
	ERegionType RegionTypeAt(int32 RegionX, int32 RegionY, int32 Seed, const FTerrainRegionTuning& Tuning);

	struct FRegionParams { float Base; float Amp; float Rough; float Steep; };
	/** Blended params from the 4 nearest region cells (smoothstep blend). */
	FRegionParams RegionParamsAt(float WorldXcm, float WorldYcm, int32 Seed, const FTerrainRegionTuning& Tuning);

	/** Regional base height H0 (DERIVED shaping formulas, flagged for approval). */
	float H0At(float WorldXcm, float WorldYcm, int32 Seed, const FTerrainRegionTuning& Tuning);

	/** Edge falloff weight in [0,1]: 1 inside, blending to 0 at the border. */
	float EdgeWeightAt(float WorldXcm, float WorldYcm, const FTerrainWorldConfig& World);
	/** H with edge falloff applied toward EdgeHeightCm (default -3000). */
	float HBoundedAt(float WorldXcm, float WorldYcm, float H, const FTerrainWorldConfig& World, float EdgeHeightCm = -3000.0f, float EdgeFalloffM = 100.0f);

	/** Deterministic source influence T at a global sample (Stage A units). */
	float SourceInfluenceAt(int32 GX, int32 GY, int32 Seed, float Strength, float RadiusSamples, int32 Salt);
	float SourceInfluence3At(int32 GX, int32 GY, int32 GZ, int32 Seed, float Strength, float RadiusSamples, int32 Salt);

	/** Hashed cave entrances near a chunk column (global coords, world cm). */
	TArray<FVector2D> EntrancesNear(int32 ChunkX, int32 ChunkY, const FTerrainWorldConfig& World, const FTerrainStageBConfig& StageB);

	/** Corridor weight in [0,1] at a world position (0 = none). */
	float CorridorWeightAt(float WorldXcm, float WorldYcm, const FTerrainCorridorInput& Corridor);

	/** Working-grid buffers for one chunk column. H/Psi cover (33+2H)^2. */
	struct FStageAWork
	{
		TArray<float> H;
		TArray<float> Psi;
		int32 W = 0; // working width
	};
	/** Init H from H0 and Psi from hash noise over the working grid. */
	void InitStageA(int32 ChunkX, int32 ChunkY, const FTerrainWorldConfig& World,
		const FTerrainRegionTuning& Regions, const FTerrainStageAConfig& StageA, FStageAWork& Out);
	/** One double-buffered iteration (fixed y,x order). Literal signs via config. */
	void StepStageA(const FStageAWork& Prev, FStageAWork& Next, const TArray<float>& CorridorW,
		const FTerrainWorldConfig& World, const FTerrainStageAConfig& StageA,
		int32 ChunkX, int32 ChunkY);
	/** Run IterationsA; returns retained center 33x33 heights. */
	TArray<float> RunStageA(int32 ChunkX, int32 ChunkY, const FTerrainWorldConfig& World,
		const FTerrainRegionTuning& Regions, const FTerrainStageAConfig& StageA,
		const FTerrainCorridorInput& Corridor);

	/** 3D working volume for the cave band of one column. */
	struct FStageBWork
	{
		TArray<float> R;
		TArray<float> PsiR;
		FIntVector Min; // global-sample-space minimum corner
		FIntVector Dim; // working dimensions
		/** Retained H (33x33) for the column, for band + gate tests. */
		TArray<float> H;
	};
	void InitStageB(int32 ChunkX, int32 ChunkY, const TArray<float>& HRetained,
		const FTerrainWorldConfig& World, const FTerrainStageBConfig& StageB, FStageBWork& Out);
	void StepStageB(const FStageBWork& Prev, FStageBWork& Next,
		const FTerrainWorldConfig& World, const FTerrainStageBConfig& StageB);
	TArray<float> RunStageB(int32 ChunkX, int32 ChunkY, const TArray<float>& HRetained,
		const FTerrainWorldConfig& World, const FTerrainStageBConfig& StageB);

	/** Carve in [0, CarveStrength] at a world point (0 outside the band). */
	float CarveAt(float WorldXcm, float WorldYcm, float WorldZcm, float H,
		const FStageBWork& B, const FTerrainWorldConfig& World, const FTerrainStageBConfig& StageB,
		const TArray<FVector2D>& Entrances);
	/** Final density F = (z - H) + Carve, with the hard air-above-margin rule. */
	float DensityAt(float WorldXcm, float WorldYcm, float WorldZcm, float H,
		const FStageBWork& B, const FTerrainWorldConfig& World, const FTerrainStageBConfig& StageB,
		const TArray<FVector2D>& Entrances);

	/** Conservative + cheap: never false when the chunk can emit triangles. */
	bool ChunkMayContainGeometry(int32 ChunkX, int32 ChunkY, int32 ChunkZ,
		const FTerrainWorldConfig& World, float HeightUncertaintyCm = 12000.0f);

	/** Canonical hash input assembly (little-endian, -0 normalized, NaN rejected). */
	struct FChunkHash
	{
		uint64 Value = 0;
		bool bValid = false;
		FString Error;
	};
	FChunkHash HashChunk(int32 ChunkX, int32 ChunkY, int32 ChunkZ,
		const FTerrainWorldConfig& World, const FTerrainRegionTuning& Regions,
		const FTerrainStageAConfig& StageA, const FTerrainStageBConfig& StageB,
		const FTerrainCorridorInput& Corridor, const TArray<float>& HRetained,
		const TArray<float>& RFinal, const FStageBWork& B);
	bool HashEqual(const FChunkHash& A, const FChunkHash& B);
	void LogHashMismatch(int32 ChunkX, int32 ChunkY, int32 ChunkZ, const FChunkHash& A, const FChunkHash& B);
	void LogTerrainHash(int32 ChunkX, int32 ChunkY, int32 ChunkZ,
		const FTerrainWorldConfig& World, const FTerrainCorridorInput& Corridor, uint64 Hash);

	/** Version-tagged generation record for stale-job rejection. */
	struct FTerrainGenRecord
	{
		FIntPoint Chunk = FIntPoint::ZeroValue;
		uint32 CorridorVersion = 0;
		uint64 Hash = 0;
		double TimeMs = 0.0;
	};
	/** True when the record predates the current corridor version. */
	inline bool IsRecordStale(const FTerrainGenRecord& Rec, const FTerrainCorridorInput& Corridor)
	{
		return Rec.CorridorVersion != Corridor.Version;
	}
}
