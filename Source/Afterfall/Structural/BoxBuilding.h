// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: FLINT (GLM session) — box-based building assembly: block layout,
// per-block physical properties (mass/durability), impact evaluation, and
// support-loss collapse. PURE module: no tick, no actor, no world state.
// Server-side callers route results into the EXISTING authority
// (AStructuralActor::ServerApplyImpact / voxel path). No second framework.
#pragma once

#include "CoreMinimal.h"
#include "StructuralTypes.h"
#include "BoxBuilding.generated.h"

/** Wall material model (graybox densities, tunable; matches destruction
 *  stack FName convention via MaterialId()). */
UENUM(BlueprintType)
enum class EBoxBlockMaterial : uint8
{
	Drywall  UMETA(DisplayName = "Drywall/Plaster"),
	Wood     UMETA(DisplayName = "Wood Frame/Board"),
	Brick    UMETA(DisplayName = "Brick/Masonry"),
	Concrete UMETA(DisplayName = "Concrete"),
	Glass    UMETA(DisplayName = "Glass Panel"),
	Metal    UMETA(DisplayName = "Metal Structural")
};

/** Block life-cycle state. */
UENUM(BlueprintType)
enum class EBoxBlockState : uint8
{
	Intact   UMETA(DisplayName = "Intact"),
	Damaged  UMETA(DisplayName = "Damaged"),
	Detached UMETA(DisplayName = "Detached"),
	Destroyed UMETA(DisplayName = "Destroyed")
};

// "4 cubes wide on ALL faces" interpretation:
// Every face (front/back/left/right/floor/ceiling) spans CubesAcrossFace x
// CubesVertical cubes; box depth is CubesDepth cubes. CORNERS ARE SHARED:
// a corner cube belongs to two orthogonal faces at ONE grid coordinate, so
// opposing faces align and corners join with no gap, overlap, or floating
// block. Openings (door/window) subtract blocks from their face rects.
// Four-wide is the DEFAULT layout only — all dimensions are configurable.

/** One structural block of the box assembly. */
USTRUCT(BlueprintType)
struct FBoxBlock
{
	GENERATED_BODY()

	/** Deterministic id: GridIndex + 1 (0 = invalid). Stable across rebuilds. */
	UPROPERTY() int32 Id = 0;
	/** Grid coordinate in cubes (x=width, y=depth, z=height). */
	UPROPERTY() FIntVector Grid = FIntVector::ZeroValue;
	UPROPERTY() EBoxBlockMaterial Material = EBoxBlockMaterial::Drywall;
	/** Accumulated damage; durability from the profile's material table. */
	UPROPERTY() float AccDamage = 0.f;
	UPROPERTY() EBoxBlockState State = EBoxBlockState::Intact;
	/** Local center relative to structure origin (cm). */
	UPROPERTY() FVector LocalCenter = FVector::ZeroVector;
};

/** Opening definition (e.g., a 2-high door at z=0..1, centered). */
USTRUCT(BlueprintType)
struct FBoxOpening
{
	GENERATED_BODY()

	/** 0=Front,1=Back,2=Left,3=Right,4=Floor,5=Ceiling. */
	UPROPERTY() int32 FaceIndex = 0;
	/** Inclusive cube rect in the face's (a,b) axes. */
	UPROPERTY() FIntPoint MinCube = FIntPoint(0, 0);
	UPROPERTY() FIntPoint MaxCube = FIntPoint(0, 0);
};

/** Box-building construction profile (data-driven; server-owned values). */
USTRUCT(BlueprintType)
struct FBoxBuildingProfile
{
	GENERATED_BODY()

	/** Cube edge in cm. Canonical 50 = the project's structural grid. */
	UPROPERTY(EditDefaultsOnly, Category = "BoxBuilding") float CubeSizeCm = 50.f;
	UPROPERTY(EditDefaultsOnly, Category = "BoxBuilding") int32 CubesAcrossFace = 4;  ///< default 4-wide per face
	UPROPERTY(EditDefaultsOnly, Category = "BoxBuilding") int32 CubesVertical = 4;
	UPROPERTY(EditDefaultsOnly, Category = "BoxBuilding") int32 CubesDepth = 4;
	UPROPERTY(EditDefaultsOnly, Category = "BoxBuilding") EBoxBlockMaterial Material = EBoxBlockMaterial::Drywall;

	UPROPERTY(EditDefaultsOnly, Category = "BoxBuilding")
		TArray<FBoxOpening> Openings;
};

/** Impact input (caller computes effective mass + relative speed). */
USTRUCT(BlueprintType)
struct FBoxImpactInput
{
	GENERATED_BODY()

	/** Effective mass in kg (throwing body, weapon, or debris). */
	UPROPERTY() float EffectiveMassKg = 75.f;
	/** Relative impact speed in cm/s (NOT m/s). */
	UPROPERTY() float RelativeSpeedCmS = 300.f;
	/** Local impact center (cm, structure-local). */
	UPROPERTY() FVector ImpactLocalCenter = FVector::ZeroVector;
	UPROPERTY() EBoxBlockMaterial Material = EBoxBlockMaterial::Drywall;
};

/** Impact evaluation result. All lists are LOCAL block ids, deterministic. */
USTRUCT(BlueprintType)
struct FBoxImpactResult
{
	GENERATED_BODY()

	UPROPERTY() int32 CenterBlockId = 0;           ///< primary damaged block
	UPROPERTY() float ImpactEnergyJ = 0.f;         ///< 0.5 * m_eff * v^2 (J)
	UPROPERTY() TArray<int32> RemovedBlocks;       ///< state -> Detached
	UPROPERTY() TArray<int32> DamagedBlocks;       ///< accumulated damage, still intact
	UPROPERTY() bool bDenied = false;              ///< energy below material threshold
	UPROPERTY() TArray<int32> CollapsedBlocks;     ///< support-loss after removals
	/** Final block state after this impact (caller commits this as the new
	 *  authoritative layout so repeated impacts accumulate). */
	UPROPERTY() TArray<FBoxBlock> UpdatedBlocks;
};

/** Pure helpers for the box assembly. */
UCLASS()
class AFTERFALL_API UBoxBuildingLib : public UObject
{
	GENERATED_BODY()

public:
	/** FName material id shared with the destruction resistance tables. */
	static FName MaterialId(EBoxBlockMaterial M);

	/** Graybox density kg/m3 per material (tunable table). */
	static float DensityKgM3(EBoxBlockMaterial M);
	/** Break energy threshold in J for ONE cube of the material. */
	static float BreakEnergyJ(EBoxBlockMaterial M);
	/** Splash damage fraction carried to neighbors (0..1). */
	static float SplashFraction(EBoxBlockMaterial M);

	/** Build the full block layout of the box: 6 faces, shared corners,
	 *  openings subtracted. Deterministic ordering (id = GridIndex+1).
	 *  FaceIndex mapping: 0=Front(-y),1=Back(+y),2=Left(-x),3=Right(+x),
	 *  4=Floor(z=0),5=Ceiling(z=V-1). CubeSizeCm converts grid->local. */
	static void BuildLayout(const FBoxBuildingProfile& Profile, TArray<FBoxBlock>& OutBlocks);

	/** True when the profile has no gaps/overlaps/floating blocks: opposing
	 *  faces span the same extent, corners shared, openings inside face. */
	static bool ValidateLayout(const FBoxBuildingProfile& Profile, const TArray<FBoxBlock>& Blocks);

	/** Mass of one block (kg) = density * volume; volume from profile cube. */
	static float BlockMassKg(const FBoxBuildingProfile& Profile, const FBoxBlock& Block);

	/** Block nearest a local position (INDEX_NONE when layout is empty). */
	static int32 NearestBlock(const TArray<FBoxBlock>& Blocks, const FVector& LocalPos);

	/**
	 * Evaluate one impact (PURE):
	 *  E = 0.5 * m_eff * v_rel^2 (J). If E < BreakEnergyJ(center block) -> denied.
	 *  Else center block detaches; neighbors within 1 cube take splash damage
	 *  (E * SplashFraction), detaching when AccDamage+damage >= durability.
	 *  Deterministic iteration order (by block id) for identical inputs.
	 */
	static FBoxImpactResult EvaluateImpact(const FBoxBuildingProfile& Profile,
		const TArray<FBoxBlock>& Blocks, const FBoxImpactInput& Impact);

	/** Graybox thin-wall mass factor per material (wall panels are thin; a
	 *  concrete cube wall core is chunky; glass is a very thin pane). */
	static float MassFactor(EBoxBlockMaterial M);
	/** Recompute support (PURE): a block is supported when it is on the floor
	 * face (anchor) OR some face-adjacent (x/y neighbor) block below it
	 * (z-1) is supported. Returns blocks that lost support after Removals —
	 * repeated until stable (bounded by block count). Anchors never fall.
	 */
	static void SupportCollapse(const FBoxBuildingProfile& Profile,
		const TArray<FBoxBlock>& Blocks, int32 RemovedId,
		TArray<int32>& OutCollapse);
};
