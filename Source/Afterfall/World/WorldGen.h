// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Integration (World generation).
//
// Reuses what already exists: FVoxelChunkData / VoxelMesher / AVoxelChunkActor
// for terrain, AStructuralActor for building shells (the one destruction path),
// and FRandomStream for deterministic placement. No second voxel, physics or
// destruction implementation is introduced.
//
// Scope honesty: the 1 km radius is the PLACEMENT + STREAMING domain. Only
// sites inside the (configurable) stream radius get geometry; the rest are
// registered places with stable ids so the domain can be verified at full
// scale without spawning 1 km of actors.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voxel/VoxelChunkTypes.h"
#include "Destruction/DestructionTypes.h"
#include "WorldGen.generated.h"

class AStructuralActor;
class AVoxelChunkActor;

/** All distances in meters; runtime conversion to Unreal centimeters is 100x. */
USTRUCT(BlueprintType)
struct FAfterfallWorldConfig
{
	GENERATED_BODY()

	/** World centre. Placement is measured from here. */
	UPROPERTY(EditAnywhere, Category = "World") FVector OriginM = FVector::ZeroVector;
	/** Horizontal generation radius: 1000 m = 1 km radius (≈3.14 km²). */
	UPROPERTY(EditAnywhere, Category = "World") float RadiusM = 1000.f;
	UPROPERTY(EditAnywhere, Category = "World") int32 Seed = 1337;
	/** One voxel edge in meters. Matches FVoxelBasis::S (3.06 m). */
	UPROPERTY(EditAnywhere, Category = "World") float VoxelSizeM = 3.06f;
	UPROPERTY(EditAnywhere, Category = "World") int32 ChunkCells = 32;
	/** Extraction: geometry exists only inside this radius. */
	UPROPERTY(EditAnywhere, Category = "World") float StreamRadiusM = 220.f;
	UPROPERTY(EditAnywhere, Category = "World") float BuildingMinSpacingM = 45.f;
	UPROPERTY(EditAnywhere, Category = "World") int32 BuildingTarget = 160;
	/** Hard cap on live voxel chunk actors (memory guard). */
	UPROPERTY(EditAnywhere, Category = "World") int32 MaxTerrainChunks = 16;
	/** Server builds the world when the map starts (no map authoring needed). */
	UPROPERTY(EditAnywhere, Category = "World") bool bGenerateOnStart = true;
	/** Keep edited chunks in Saved/WorldGen so damage survives a restart. */
	UPROPERTY(EditAnywhere, Category = "World") bool bPersistChunkEdits = true;

	bool Validate(FString& OutError) const;
	float ChunkSizeM() const { return VoxelSizeM * (float)ChunkCells; }

	/** Phase 2: house placement on the chunk grid. */
	/** One house may span multiple chunks (true) or must fit in one (false). */
	UPROPERTY(EditAnywhere, Category = "World|Houses") bool bHousesMaySpanChunks = true;
	/** Complete empty chunks between any two houses (the 5-block gap rule). */
	UPROPERTY(EditAnywhere, Category = "World|Houses") int32 HouseGapChunks = 5;
	/** Target house count (capped by radius / gap math). */
	UPROPERTY(EditAnywhere, Category = "World|Houses") int32 HouseTarget = 40;
};

/** One house template: rooms, doors, furniture placements (all in local space). */
USTRUCT()
struct FHouseBlueprint
{
	GENERATED_BODY()

	UPROPERTY() FName ArchetypeName = NAME_None;  // "house", "shop", "warehouse"
	UPROPERTY() float FootprintM = 0.f;      // overall width/depth in meters (square)
	UPROPERTY() int32 RoomCount = 0;       // number of rooms
	UPROPERTY() TArray<FVector> RoomCentersM;   // local room centers
	UPROPERTY() TArray<FVector> DoorPositionsM; // local door positions (on perimeter)
	UPROPERTY() TArray<FVector> FurniturePositionsM; // local furniture spots
	UPROPERTY() EMaterialType PrimaryMaterial = EMaterialType::Wood;  // wood/stone/etc
};

/** Chunk-aligned house site: one house per chunk anchor, 5-block gap rule. */
USTRUCT()
struct FHousePlacement
{
	GENERATED_BODY()

	UPROPERTY() FGuid StableId = FGuid();
	UPROPERTY() FIntPoint ChunkCoord = FIntPoint::ZeroValue;  // chunk this house is anchored to
	UPROPERTY() FVector LocationM = FVector::ZeroVector;  // world location in meters
	UPROPERTY() FHouseBlueprint Blueprint;
	/** All chunks the house footprint touches (for multi-chunk buildings). */
	UPROPERTY() TArray<FIntPoint> OccupiedChunks;
};

/** Placement rule: N empty chunks between any two houses. */
USTRUCT()
struct FHousePlacementRule
{
	GENERATED_BODY()

	UPROPERTY() int32 MinEmptyChunksBetween = 5;  // 5 complete empty blocks
	UPROPERTY() bool bAllowMultiChunkBuildings = true;
	UPROPERTY() float MaxFootprintM = 24.f;  // warehouse max footprint
};

/** One registered building site. Ids are stable for (Seed, Index). */
USTRUCT()
struct FAfterfallBuildingSite
{
	GENERATED_BODY()

	UPROPERTY() FGuid StableId = FGuid();
	UPROPERTY() FVector LocationM = FVector::ZeroVector;
	UPROPERTY() int32 Archetype = 0; // 0 house, 1 shop, 2 warehouse
	UPROPERTY() float FootprintM = 12.f;
	UPROPERTY() bool bHasGeometry = false;
};

/** Server-side restore record for one streamed-out chunk. Not replicated. */
struct FAfterfallPersistedChunk
{
	FVoxelChunkData Data;
	int32 Seq = 0;
};

UCLASS()
class AFTERFALL_API UAfterfallWorldGenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, Category = "World") FAfterfallWorldConfig Config;

	UPROPERTY() TArray<FAfterfallBuildingSite> Sites;
	/** Phase 2: chunk-aligned house placements with gap rule. */
	UPROPERTY() TArray<FHousePlacement> HousePlacements;
	/** Weak refs: spawned actors are owned by the world, not by this subsystem. */
	TArray<TWeakObjectPtr<AStructuralActor>> BuildingParts;

	/** Terrain chunks currently spawned, keyed by lattice chunk coordinate. */
	TMap<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>> LoadedChunks;

	/** Edited chunk state kept while the chunk is streamed out (walk back = intact damage). */
	TMap<FIntPoint, FAfterfallPersistedChunk> PersistedChunks;
	TArray<FIntPoint> PersistedOrder;

	/** Deterministic site list for a config (pure apart from the RNG stream). */
	static TArray<FAfterfallBuildingSite> PlanSites(const FAfterfallWorldConfig& In);
	static FGuid StableIdFor(int32 Seed, int32 Index);
	static bool InsideRadius(const FVector& OriginM, const FVector& SiteM, float RadiusM);

	// ---- Phase 2: multi-chunk house placement ----

	/** Deterministic house list on the chunk grid with N-block gap rule. */
	TArray<FHousePlacement> PlanHouses(const FAfterfallWorldConfig& In);
	/** All chunks a house footprint at LocationM with FootprintM touches. */
	TArray<FIntPoint> HouseFootprintChunks(const FAfterfallWorldConfig& In, const FVector& LocationM, float FootprintM);
	/** True when two placements respect the gap rule (no chunk within MinEmptyChunksBetween of the other). */
	static bool PlacementsRespectGap(const FHousePlacement& A, const FHousePlacement& B, const FAfterfallWorldConfig& In);
	/** Check neighboring chunks before placing: no house may overlap another's footprint. */
	static bool ChunkReflectionOk(const FHousePlacement& Candidate, const TArray<FHousePlacement>& Existing, const FAfterfallWorldConfig& In);

	/** House blueprint templates by archetype. */
	static FHouseBlueprint HouseBlueprint(FName Archetype);
	/** Spawn rooms, doors, furniture inside an existing building shell. */
	void GenerateHouseInterior(const FVector& CenterCm, const FHousePlacement& Placement);

	/** Destroys everything this subsystem spawned; returns the actor count. */
	int32 ClearGenerated();

	/** Server only: plan + spawn building shells, then stream terrain at the focus. */
	bool GenerateWorld();

	/**
	 * Server only: (re)build the chunk ring around FocusCm. Chunks outside the
	 * stream radius are unloaded, missing ones are spawned/restored. Loaded
	 * count is capped by MaxTerrainChunks (nearest chunks win). Returns the live
	 * chunk count so tests can assert on it.
	 */
	int32 UpdateStreamingAt(const FVector& FocusCm, bool bForce = false);

	/**
	 * Same, for every player at once (a listen server / dedicated server with
	 * several players). Chunks are ranked by their distance to the NEAREST
	 * focus, so the cap is shared fairly instead of one player owning it.
	 */
	int32 UpdateStreamingForFocuses(const TArray<FVector>& FocusesCm, bool bForce = false);

	/** Starts the periodic stream update (space in server-side streaming). */
	void StartStreaming();

	FIntPoint ChunkCoordForWorld(const FVector& WorldCm) const;
	FVector ChunkCornerWorldCm(const FIntPoint& Coord) const;

	/** Saved/WorldGen/Chunks_<seed>.afw — seed-specific, so worlds never mix. */
	FString PersistFilePath() const;
	/** Writes every edited chunk (live + streamed-out) to disk. */
	bool SavePersistedChunks();
	/** Loads edited chunks from disk into the restore cache; returns the count. */
	int32 LoadPersistedChunks();

	/** Headless acceptance: plan twice, generate, break a wall, report. */
	bool RunSelfTest(bool bExit);

	int32 TerrainChunkCount = 0;
	int32 BuildingCount = 0;
	int32 BuildingPartCount = 0;
	int32 ChunksStreamedIn = 0;
	int32 ChunksStreamedOut = 0;
	int32 ChunksRestored = 0;
	float PlanMs = 0.f;
	float GenerateMs = 0.f;
	FVector StreamFocusCm = FVector::ZeroVector;
	TArray<FVector> StreamFocusesCm;

	/** Bounded restore cache: 64 chunks x 32 KB = 2 MB of edited state. */
	int32 PersistedCacheCap = 64;
	/** 2 Hz is enough for 98 m chunks walked at run speed. */
	float StreamIntervalS = 0.5f;
	/** Set when an edited chunk leaves memory; flushed once per stream update. */
	bool bPersistDirty = false;
	int32 PersistedChunksLoadedFromDisk = 0;

private:
	FTimerHandle AutoTimer;
	FTimerHandle StreamTimer;
	void AutoRun();
	void StreamTick();
	bool SpawnChunk(const FIntPoint& Coord);
	void UnloadChunk(const FIntPoint& Coord);
};
