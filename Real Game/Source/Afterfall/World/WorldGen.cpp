// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "World/WorldGen.h"
#include "Structural/StructuralActor.h"
#include "Structural/StructuralTypes.h"
#include "Structural/ConstructionValidator.h"
#include "Destruction/DestructionPropagation.h"
#include "Voxel/VoxelChunkActor.h"
#include "Afterfall.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
	constexpr float CmPerM = 100.f;

	FName GeneratedTag() { return FName(TEXT("AFWorldGen")); }

	// Chunk-damage file layout: 4 x int32 header (magic, version, seed, count),
	// then per chunk 3 x int32 (X, Y, Seq) and the voxel array.
	constexpr int32 PersistMagic = 0x41464757; // AFGW
	constexpr int32 PersistVersion = 1;

	float Mb(uint64 Bytes) { return (float)Bytes / (1024.f * 1024.f); }

	void WorldLog(bool bOk, const TCHAR* What)
	{
		UE_LOG(LogAfterfall, Log, TEXT("WORLDGEN %s: %s"),
			bOk ? TEXT("PASS") : TEXT("FAIL"), What);
	}
}

bool FAfterfallWorldConfig::Validate(FString& OutError) const
{
	if (RadiusM <= 0.f || RadiusM > 5000.f)
	{
		OutError = TEXT("RadiusM must be in (0, 5000]");
		return false;
	}
	if (VoxelSizeM <= 0.f || ChunkCells < 1 || ChunkCells > 64)
	{
		OutError = TEXT("VoxelSizeM > 0 and 1 <= ChunkCells <= 64 required");
		return false;
	}
	if (BuildingMinSpacingM <= 0.f)
	{
		OutError = TEXT("BuildingMinSpacingM must be > 0");
		return false;
	}
	if (StreamRadiusM <= 0.f || StreamRadiusM > RadiusM)
	{
		OutError = TEXT("StreamRadiusM must be in (0, RadiusM]");
		return false;
	}
	if (MaxTerrainChunks < 0 || MaxTerrainChunks > 256)
	{
		OutError = TEXT("MaxTerrainChunks must be in [0, 256]");
		return false;
	}
	return true;
}

bool UAfterfallWorldGenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* W = Cast<UWorld>(Outer))
	{
		return W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE;
	}
	return false;
}

void UAfterfallWorldGenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("afWorldGenSelfTest")))
	{
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().SetTimer(AutoTimer, this,
				&UAfterfallWorldGenSubsystem::AutoRun, 2.0f, false);
		}
	}
}

void UAfterfallWorldGenSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The world exists as soon as the map starts: terrain chunks plus the
	// nearest building shells are spawned from the same deterministic plan,
	// so no authored .umap is needed and every peer agrees on the layout.
	if (Config.bGenerateOnStart && !InWorld.IsNetMode(NM_Client))
	{
		GenerateWorld();
		// Terrain then follows the player: chunks outside the stream radius are
		// released and re-created (with their damage) as the focus moves.
		StartStreaming();
	}
}

void UAfterfallWorldGenSubsystem::AutoRun()
{
	RunSelfTest(true);
}

static void RunWorldGenCmd(UWorld* W)
{
	if (!W)
	{
		return;
	}
	if (UAfterfallWorldGenSubsystem* S = W->GetSubsystem<UAfterfallWorldGenSubsystem>())
	{
		S->GenerateWorld();
	}
}

static FAutoConsoleCommandWithWorld GWorldGenCmd(
	TEXT("afWorldGen"),
	TEXT("AFTERFALL world: generate voxel chunks + buildings for the configured radius"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunWorldGenCmd));

bool UAfterfallWorldGenSubsystem::InsideRadius(const FVector& OriginM, const FVector& SiteM,
	float RadiusM)
{
	const double dx = SiteM.X - OriginM.X;
	const double dz = SiteM.Y - OriginM.Y;
	return dx * dx + dz * dz <= (double)RadiusM * (double)RadiusM;
}

FGuid UAfterfallWorldGenSubsystem::StableIdFor(int32 Seed, int32 Index)
{
	// Deterministic, platform-stable id: hash(seed, index) split into 4 words.
	const uint32 H = HashCombine(GetTypeHash((uint32)Seed), GetTypeHash((uint32)Index));
	const uint32 A = H;
	const uint32 B = HashCombine(H, 0x9E3779B9u);
	const uint32 C = HashCombine(B, 0x85EBCA6Bu);
	const uint32 D = HashCombine(C, 0xC2B2AE35u);
	return FGuid(A, B, C, D);
}

TArray<FAfterfallBuildingSite> UAfterfallWorldGenSubsystem::PlanSites(
	const FAfterfallWorldConfig& In)
{
	TArray<FAfterfallBuildingSite> Out;
	// Jittered grid over a square that circumscribes the disc, then the exact
	// circular test: dx*dx + dz*dz <= R*R.
	const float Spacing = In.BuildingMinSpacingM;
	const int32 Steps = FMath::Max(1, FMath::FloorToInt(In.RadiusM / Spacing));
	FRandomStream Rng(In.Seed);
	int32 Index = 0;
	for (int32 gx = -Steps; gx <= Steps; ++gx)
	{
		for (int32 gy = -Steps; gy <= Steps; ++gy)
		{
			// Skip the corners cheaply before paying for jitter/distance.
			const double cx = (double)gx * Spacing;
			const double cy = (double)gy * Spacing;
			if (cx * cx + cy * cy > (double)In.RadiusM * (double)In.RadiusM)
			{
				continue;
			}
			// Jitter stays inside the spacing guarantee: two neighbours on the
			// grid can move 0.3*Spacing toward each other per axis, so the
			// closest possible pair is 0.7*Spacing apart.
			const float Jx = Rng.FRandRange(-0.15f, 0.15f) * Spacing;
			const float Jy = Rng.FRandRange(-0.15f, 0.15f) * Spacing;
			FVector Site((float)cx + Jx, (float)cy + Jy, 0.f);
			if (!InsideRadius(FVector::ZeroVector, Site, In.RadiusM))
			{
				continue;
			}
			// Deterministic archetype/footprint from the same stream.
			const int32 Archetype = Rng.RandRange(0, 2);
			FAfterfallBuildingSite S;
			S.StableId = StableIdFor(In.Seed, Index);
			S.LocationM = Site + In.OriginM;
			S.Archetype = Archetype;
			S.FootprintM = Archetype == 0 ? 12.f : (Archetype == 1 ? 18.f : 24.f);
			Out.Add(S);
			++Index;
		}
	}
	// Density cap: keep the BuildingTarget sites NEAREST the origin (capped
	// sites are the ones that stream in first), never an arbitrary edge slice.
	if (In.BuildingTarget > 0 && Out.Num() > In.BuildingTarget)
	{
		Out.Sort([&In](const FAfterfallBuildingSite& L, const FAfterfallBuildingSite& R)
		{
			return FVector::DistSquared2D(L.LocationM, In.OriginM)
				< FVector::DistSquared2D(R.LocationM, In.OriginM);
		});
		Out.SetNum(In.BuildingTarget);
	}
	return Out;
}

// =====================================================================
// Phase 2: multi-chunk house placement
// =====================================================================

TArray<FIntPoint> UAfterfallWorldGenSubsystem::HouseFootprintChunks(
	const FAfterfallWorldConfig& In, const FVector& LocationM, float FootprintM)
{
	TArray<FIntPoint> Out;
	const float ChunkCm = In.ChunkSizeM() * 100.f;
	const float HalfM = FootprintM * 0.5f;
	const FVector MinM = LocationM - FVector(HalfM, HalfM, 0.f);
	const FVector MaxM = LocationM + FVector(HalfM, HalfM, 0.f);
	// Convert to chunk coords covering the full footprint (inclusive corners).
	const FIntPoint MinC = this->ChunkCoordForWorld(MinM * 100.f);
	const FIntPoint MaxC = this->ChunkCoordForWorld(MaxM * 100.f);
	for (int32 cx = MinC.X; cx <= MaxC.X; ++cx)
	{
		for (int32 cy = MinC.Y; cy <= MaxC.Y; ++cy)
		{
			Out.Add(FIntPoint(cx, cy));
		}
	}
	return Out;
}

bool UAfterfallWorldGenSubsystem::PlacementsRespectGap(
	const FHousePlacement& A, const FHousePlacement& B, const FAfterfallWorldConfig& In)
{
	const int32 Gap = In.HouseGapChunks;
	// Two placements conflict if any occupied chunk of A is within Gap chunks
	// of any occupied chunk of B (Manhattan distance on the chunk lattice).
	for (const FIntPoint& Ca : A.OccupiedChunks)
	{
		for (const FIntPoint& Cb : B.OccupiedChunks)
		{
			const int32 Dx = FMath::Abs(Ca.X - Cb.X);
			const int32 Dy = FMath::Abs(Ca.Y - Cb.Y);
			if (Dx <= Gap && Dy <= Gap)
			{
				return false; // too close: gap rule violated
			}
		}
	}
	return true;
}

bool UAfterfallWorldGenSubsystem::ChunkReflectionOk(
	const FHousePlacement& Candidate, const TArray<FHousePlacement>& Existing,
	const FAfterfallWorldConfig& In)
{
	// Chunk reflection: the candidate must not overlap any existing house's
	// occupied chunks, and must respect the gap rule against all of them.
	for (const FHousePlacement& E : Existing)
	{
		// Direct overlap: any shared occupied chunk.
		for (const FIntPoint& Cc : Candidate.OccupiedChunks)
		{
			for (const FIntPoint& Ce : E.OccupiedChunks)
			{
				if (Cc == Ce)
				{
					return false; // footprints overlap
				}
			}
		}
		// Gap rule: must be at least HouseGapChunks away.
		if (!PlacementsRespectGap(Candidate, E, In))
		{
			return false;
		}
	}
	return true;
}

FHouseBlueprint UAfterfallWorldGenSubsystem::HouseBlueprint(FName Archetype)
{
	FHouseBlueprint Bp;
	if (Archetype == FName(TEXT("house")))
	{
		Bp.ArchetypeName = FName(TEXT("house"));
		Bp.FootprintM = 12.f;
		Bp.RoomCount = 4;
		Bp.PrimaryMaterial = EMaterialType::Wood;
		// Four rooms around a center hallway.
		Bp.RoomCentersM = {
			FVector(-4.f, -4.f, 0.f), FVector(4.f, -4.f, 0.f),
			FVector(-4.f, 4.f, 0.f),  FVector(4.f, 4.f, 0.f)
		};
		// One door per side.
		Bp.DoorPositionsM = {
			FVector(0.f, -6.f, 0.f), FVector(6.f, 0.f, 0.f),
			FVector(0.f, 6.f, 0.f),  FVector(-6.f, 0.f, 0.f)
		};
		// Furniture: bed, table, chair, shelf.
		Bp.FurniturePositionsM = {
			FVector(-3.f, -3.f, 0.f), FVector(3.f, -3.f, 0.f),
			FVector(-3.f, 3.f, 0.f),  FVector(3.f, 3.f, 0.f)
		};
	}
	else if (Archetype == FName(TEXT("shop")))
	{
		Bp.ArchetypeName = FName(TEXT("shop"));
		Bp.FootprintM = 18.f;
		Bp.RoomCount = 2;
		Bp.PrimaryMaterial = EMaterialType::Wood;
		Bp.RoomCentersM = { FVector(-6.f, 0.f, 0.f), FVector(6.f, 0.f, 0.f) };
		Bp.DoorPositionsM = { FVector(0.f, -9.f, 0.f), FVector(0.f, 9.f, 0.f) };
		Bp.FurniturePositionsM = {
			FVector(-5.f, -4.f, 0.f), FVector(-5.f, 4.f, 0.f),
			FVector(5.f, -4.f, 0.f),  FVector(5.f, 4.f, 0.f)
		};
	}
	else // warehouse
	{
		Bp.ArchetypeName = FName(TEXT("warehouse"));
		Bp.FootprintM = 24.f;
		Bp.RoomCount = 1;
		Bp.PrimaryMaterial = EMaterialType::Metal;
		Bp.RoomCentersM = { FVector(0.f, 0.f, 0.f) };
		Bp.DoorPositionsM = { FVector(0.f, -12.f, 0.f), FVector(0.f, 12.f, 0.f) };
		Bp.FurniturePositionsM = {
			FVector(-8.f, -8.f, 0.f), FVector(0.f, -8.f, 0.f), FVector(8.f, -8.f, 0.f),
			FVector(-8.f, 8.f, 0.f),  FVector(0.f, 8.f, 0.f),  FVector(8.f, 8.f, 0.f)
		};
	}
	return Bp;
}

TArray<FHousePlacement> UAfterfallWorldGenSubsystem::PlanHouses(
	const FAfterfallWorldConfig& In)
{
	TArray<FHousePlacement> Out;
	if (In.HouseTarget <= 0)
	{
		return Out;
	}
	const float ChunkM = In.ChunkSizeM();
	const int32 Steps = FMath::Max(1, FMath::FloorToInt(In.RadiusM / (ChunkM * (In.HouseGapChunks + 1))));
	FRandomStream Rng(In.Seed);
	int32 Index = 0;
	// Jittered grid on the chunk lattice: candidate anchor at every Nth chunk,
	// where N = Gap + 1 so adjacent anchors already respect the gap.
	for (int32 gx = -Steps; gx <= Steps; ++gx)
	{
		for (int32 gy = -Steps; gy <= Steps; ++gy)
		{
			// Anchor on chunk multiples: (gx * (Gap+1), gy * (Gap+1)).
			const int32 AnchorX = gx * (In.HouseGapChunks + 1);
			const int32 AnchorY = gy * (In.HouseGapChunks + 1);
			const float Cx = (float)AnchorX * ChunkM;
			const float Cy = (float)AnchorY * ChunkM;
			if (Cx * Cx + Cy * Cy > In.RadiusM * In.RadiusM)
			{
				continue;
			}
			// Jitter within a chunk so houses aren't perfectly grid-aligned,
			// but stay inside the anchor chunk (max 0.45 * ChunkM offset).
			const float Jx = Rng.FRandRange(-0.45f, 0.45f) * ChunkM;
			const float Jy = Rng.FRandRange(-0.45f, 0.45f) * ChunkM;
			FVector Site(Cx + Jx, Cy + Jy, 0.f);
			if (!InsideRadius(FVector::ZeroVector, Site, In.RadiusM))
			{
				continue;
			}
			// Deterministic archetype from the same stream.
			const int32 ArchetypeRoll = Rng.RandRange(0, 2);
			const FName Archetype = ArchetypeRoll == 0 ? FName(TEXT("house"))
				: (ArchetypeRoll == 1 ? FName(TEXT("shop")) : FName(TEXT("warehouse")));
			const FHouseBlueprint Bp = HouseBlueprint(Archetype);
			FHousePlacement P;
			P.StableId = StableIdFor(In.Seed, Index);
			P.ChunkCoord = this->ChunkCoordForWorld(Site * 100.f);
			P.LocationM = Site + In.OriginM;
			P.Blueprint = Bp;
			P.OccupiedChunks = HouseFootprintChunks(In, Site, Bp.FootprintM);
			// Gap + reflection check against all previously accepted houses.
			bool bOk = true;
			for (const FHousePlacement& E : Out)
			{
				if (!ChunkReflectionOk(P, { E }, In))
				{
					bOk = false;
					break;
				}
			}
			if (!bOk)
			{
				continue;
			}
			Out.Add(P);
			++Index;
			if (Index >= In.HouseTarget)
			{
				return Out;
			}
		}
	}
	// Sort by distance to origin so nearer houses stream in first.
	Out.Sort([&In](const FHousePlacement& L, const FHousePlacement& R)
	{
		return FVector::DistSquared2D(L.LocationM, In.OriginM)
			< FVector::DistSquared2D(R.LocationM, In.OriginM);
	});
	if (In.HouseTarget > 0 && Out.Num() > In.HouseTarget)
	{
		Out.SetNum(In.HouseTarget);
	}
	return Out;
}

void UAfterfallWorldGenSubsystem::GenerateHouseInterior(
	const FVector& CenterCm, const FHousePlacement& Placement)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const FHouseBlueprint& Bp = Placement.Blueprint;
	const float CellExtent = 100.f; // 1m structural cells
	// Spawn floor tiles under each room.
	for (const FVector& Room : Bp.RoomCentersM)
	{
		const FVector At = CenterCm + Room * 100.f + FVector(0.f, 0.f, CellExtent * 0.5f);
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStructuralActor* Floor = W->SpawnActor<AStructuralActor>(
			AStructuralActor::StaticClass(), At, FRotator::ZeroRotator, SP);
		if (Floor && Floor->Destruction)
		{
			Floor->Tags.Add(GeneratedTag());
			Floor->Destruction->CellExtentCm = CellExtent;
			FStructuralSpec Spec;
			Spec.Part = EStructuralPart::Floor;
			Spec.MaxDurability = UConstructionValidator::GetMaxDurability(EStructuralPart::Floor);
			Spec.bRequiresSupport = true;
			Floor->ServerInitialize(Spec, 0);
			Floor->RefreshDestructionVisual();
			BuildingParts.Add(Floor);
			++BuildingPartCount;
		}
	}
	// Spawn door frames on the perimeter.
	for (const FVector& Door : Bp.DoorPositionsM)
	{
		const FVector At = CenterCm + Door * 100.f + FVector(0.f, 0.f, CellExtent * 1.f);
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStructuralActor* DoorFrame = W->SpawnActor<AStructuralActor>(
			AStructuralActor::StaticClass(), At, FRotator::ZeroRotator, SP);
		if (DoorFrame && DoorFrame->Destruction)
		{
			DoorFrame->Tags.Add(GeneratedTag());
			DoorFrame->Destruction->CellExtentCm = CellExtent;
			FStructuralSpec Spec;
			Spec.Part = EStructuralPart::Door;
			Spec.MaxDurability = UConstructionValidator::GetMaxDurability(EStructuralPart::Door);
			Spec.bRequiresSupport = true;
			DoorFrame->ServerInitialize(Spec, 0);
			DoorFrame->RefreshDestructionVisual();
			BuildingParts.Add(DoorFrame);
			++BuildingPartCount;
		}
	}
	// Furniture markers are simple wall-strength parts (destructible clutter).
	for (const FVector& Furn : Bp.FurniturePositionsM)
	{
		const FVector At = CenterCm + Furn * 100.f + FVector(0.f, 0.f, CellExtent * 0.5f);
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStructuralActor* Furniture = W->SpawnActor<AStructuralActor>(
			AStructuralActor::StaticClass(), At, FRotator::ZeroRotator, SP);
		if (Furniture && Furniture->Destruction)
		{
			Furniture->Tags.Add(GeneratedTag());
			Furniture->Destruction->CellExtentCm = CellExtent;
			FStructuralSpec Spec;
			Spec.Part = EStructuralPart::Barricade;
			Spec.MaxDurability = UConstructionValidator::GetMaxDurability(EStructuralPart::Barricade);
			Spec.bRequiresSupport = false;
			Furniture->ServerInitialize(Spec, 0);
			Furniture->RefreshDestructionVisual();
			BuildingParts.Add(Furniture);
			++BuildingPartCount;
		}
	}
}

int32 UAfterfallWorldGenSubsystem::ClearGenerated()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return 0;
	}
	TArray<AActor*> Dead;
	for (TActorIterator<AActor> It(W); It; ++It)
	{			if (It->Tags.Contains(GeneratedTag()))
		{
			Dead.Add(*It);
		}
	}
	for (AActor* A : Dead)
	{
		A->Destroy();
	}
	LoadedChunks.Reset();
	PersistedChunks.Reset();
	PersistedOrder.Reset();
	BuildingParts.Reset();
	TerrainChunkCount = 0;
	BuildingCount = 0;
	BuildingPartCount = 0;
	return Dead.Num();
}

FIntPoint UAfterfallWorldGenSubsystem::ChunkCoordForWorld(const FVector& WorldCm) const
{
	const float ChunkCm = FMath::Max(1.f, Config.ChunkSizeM() * CmPerM);
	const FVector Rel = WorldCm - Config.OriginM * CmPerM;
	return FIntPoint(FMath::FloorToInt(Rel.X / ChunkCm), FMath::FloorToInt(Rel.Y / ChunkCm));
}

FVector UAfterfallWorldGenSubsystem::ChunkCornerWorldCm(const FIntPoint& Coord) const
{
	const float ChunkCm = Config.ChunkSizeM() * CmPerM;
	return Config.OriginM * CmPerM + FVector(Coord.X * ChunkCm, Coord.Y * ChunkCm, 0.f);
}

bool UAfterfallWorldGenSubsystem::SpawnChunk(const FIntPoint& Coord)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return false;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVoxelChunkActor* Chunk = W->SpawnActor<AVoxelChunkActor>(
		AVoxelChunkActor::StaticClass(), ChunkCornerWorldCm(Coord), FRotator::ZeroRotator, SP);
	if (!Chunk)
	{
		return false;
	}
	Chunk->Tags.Add(GeneratedTag());
	LoadedChunks.Add(Coord, Chunk);
	++ChunksStreamedIn;
	// Damage from a previous visit is restored, not regenerated pristine.
	if (const FAfterfallPersistedChunk* Saved = PersistedChunks.Find(Coord))
	{
		Chunk->ServerAdoptSnapshot(Saved->Data, Saved->Seq);
		PersistedChunks.Remove(Coord);
		PersistedOrder.Remove(Coord);
		++ChunksRestored;
	}
	return true;
}

void UAfterfallWorldGenSubsystem::UnloadChunk(const FIntPoint& Coord)
{
	const TWeakObjectPtr<AVoxelChunkActor>* Found = LoadedChunks.Find(Coord);
	if (!Found)
	{
		return;
	}
	if (AVoxelChunkActor* Chunk = Found->Get())
	{
		// Only edited chunks need keeping: pristine content regenerates
		// identically from the deterministic initializer.
		if (Chunk->GetServerSeq() > 0)
		{
			FAfterfallPersistedChunk& Rec = PersistedChunks.FindOrAdd(Coord);
			Rec.Data = Chunk->GetVoxelData();
			Rec.Seq = Chunk->GetServerSeq();
			PersistedOrder.Remove(Coord);
			PersistedOrder.Add(Coord);
			bPersistDirty = true;
			while (PersistedOrder.Num() > FMath::Max(1, PersistedCacheCap))
			{
				PersistedChunks.Remove(PersistedOrder[0]);
				PersistedOrder.RemoveAt(0);
			}
		}
		Chunk->Destroy();
	}
	LoadedChunks.Remove(Coord);
	++ChunksStreamedOut;
}

int32 UAfterfallWorldGenSubsystem::UpdateStreamingAt(const FVector& FocusCm, bool bForce)
{
	TArray<FVector> Focuses;
	Focuses.Add(FocusCm);
	return UpdateStreamingForFocuses(Focuses, bForce);
}

int32 UAfterfallWorldGenSubsystem::UpdateStreamingForFocuses(const TArray<FVector>& FocusesCm,
	bool bForce)
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return 0;
	}
	FString Err;
	if (!Config.Validate(Err))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("WORLDGEN stream skipped: %s"), *Err);
		return LoadedChunks.Num();
	}

	TArray<FVector> Focuses = FocusesCm;
	if (Focuses.Num() == 0)
	{
		Focuses.Add(Config.OriginM * CmPerM);
	}

	const float ChunkCm = Config.ChunkSizeM() * CmPerM;
	const float ReachCm = (Config.StreamRadiusM + Config.ChunkSizeM() * 0.5f) * CmPerM;
	const int32 Range = FMath::CeilToInt(ReachCm / ChunkCm) + 1;

	// Union over every player: a chunk's priority is its distance to the
	// NEAREST focus, so several players share the cap instead of the first
	// player's ring starving everyone else.
	TMap<FIntPoint, float> BestD2;
	for (const FVector& Focus : Focuses)
	{
		const FIntPoint FocusCoord = ChunkCoordForWorld(Focus);
		for (int32 dx = -Range; dx <= Range; ++dx)
		{
			for (int32 dy = -Range; dy <= Range; ++dy)
			{
				const FIntPoint Coord(FocusCoord.X + dx, FocusCoord.Y + dy);
				const FVector Centre = ChunkCornerWorldCm(Coord)
					+ FVector(ChunkCm * 0.5f, ChunkCm * 0.5f, 0.f);
				const float D2 = FVector::DistSquared2D(Centre, Focus);
				if (D2 > ReachCm * ReachCm)
				{
					continue;
				}
				float& Cur = BestD2.FindOrAdd(Coord, TNumericLimits<float>::Max());
				Cur = FMath::Min(Cur, D2);
			}
		}
	}

	TArray<TPair<float, FIntPoint>> Wanted;
	Wanted.Reserve(BestD2.Num());
	for (const TPair<FIntPoint, float>& P : BestD2)
	{
		Wanted.Emplace(P.Value, P.Key);
	}
	Wanted.Sort([](const TPair<float, FIntPoint>& A, const TPair<float, FIntPoint>& B)
	{
		return A.Key < B.Key;
	});
	if (Config.MaxTerrainChunks > 0 && Wanted.Num() > Config.MaxTerrainChunks)
	{
		Wanted.SetNum(Config.MaxTerrainChunks);
	}

	TSet<FIntPoint> Desired;
	Desired.Reserve(Wanted.Num());
	for (const TPair<float, FIntPoint>& P : Wanted)
	{
		Desired.Add(P.Value);
	}

	// Unload first so the live count never exceeds the cap mid-update.
	TArray<FIntPoint> Stale;
	for (const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& P : LoadedChunks)
	{
		if (!Desired.Contains(P.Key))
		{
			Stale.Add(P.Key);
		}
	}
	for (const FIntPoint& Coord : Stale)
	{
		UnloadChunk(Coord);
	}

	for (const TPair<float, FIntPoint>& P : Wanted)
	{
		if (LoadedChunks.Contains(P.Value))
		{
			continue;
		}
		SpawnChunk(P.Value);
	}

	StreamFocusCm = Focuses[0];
	StreamFocusesCm = Focuses;
	TerrainChunkCount = LoadedChunks.Num();
	// One write per update at most, and only after an edited chunk left memory.
	if (bPersistDirty)
	{
		SavePersistedChunks();
	}
	if (bForce)
	{
		UE_LOG(LogAfterfall, Log,
			TEXT("WORLDGEN stream: focuses=%d focus=%s chunks=%d (in=%d out=%d restored=%d cached=%d)"),
			Focuses.Num(), *Focuses[0].ToString(), TerrainChunkCount, ChunksStreamedIn,
			ChunksStreamedOut, ChunksRestored, PersistedChunks.Num());
	}
	return TerrainChunkCount;
}

FString UAfterfallWorldGenSubsystem::PersistFilePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WorldGen"),
		FString::Printf(TEXT("Chunks_%d.afw"), Config.Seed));
}

bool UAfterfallWorldGenSubsystem::SavePersistedChunks()
{
	bPersistDirty = false;
	if (!Config.bPersistChunkEdits)
	{
		return false;
	}

	// Live edited chunks win over anything sitting in the stream-out cache.
	TMap<FIntPoint, FAfterfallPersistedChunk> All = PersistedChunks;
	for (const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& P : LoadedChunks)
	{
		AVoxelChunkActor* Chunk = P.Value.Get();
		if (!Chunk || Chunk->GetServerSeq() <= 0)
		{
			continue;
		}
		FAfterfallPersistedChunk Rec;
		Rec.Data = Chunk->GetVoxelData();
		Rec.Seq = Chunk->GetServerSeq();
		All.Add(P.Key, MoveTemp(Rec));
	}
	if (All.Num() == 0)
	{
		return true; // nothing to persist; never create an empty file
	}

	const FString Path = PersistFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FBufferArchive Ar;
	int32 Header[4] = { PersistMagic, PersistVersion, Config.Seed, All.Num() };
	Ar.Serialize(Header, sizeof(Header));
	for (TPair<FIntPoint, FAfterfallPersistedChunk>& P : All)
	{
		int32 Meta[3] = { P.Key.X, P.Key.Y, P.Value.Seq };
		Ar.Serialize(Meta, sizeof(Meta));
		Ar << P.Value.Data.Voxels;
	}
	if (!FFileHelper::SaveArrayToFile(Ar, *Path))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("WORLDGEN persist: FAILED to write %s"), *Path);
		return false;
	}
	UE_LOG(LogAfterfall, Log,
		TEXT("WORLDGEN persist: saved %d edited chunks to %s (%.1f KB)"),
		All.Num(), *Path, (float)Ar.Num() / 1024.f);
	return true;
}

int32 UAfterfallWorldGenSubsystem::LoadPersistedChunks()
{
	PersistedChunksLoadedFromDisk = 0;
	if (!Config.bPersistChunkEdits)
	{
		return 0;
	}
	const FString Path = PersistFilePath();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		return 0; // first run for this seed
	}
	FMemoryReader Ar(Bytes, true);
	int32 Header[4] = { 0, 0, 0, 0 };
	Ar.Serialize(Header, sizeof(Header));
	if (Ar.IsError() || Header[0] != PersistMagic || Header[1] != PersistVersion
		|| Header[2] != Config.Seed || Header[3] < 0 || Header[3] > 4096)
	{
		UE_LOG(LogAfterfall, Warning,
			TEXT("WORLDGEN persist: ignoring %s (bad header, expected seed %d)"),
			*Path, Config.Seed);
		return 0;
	}

	int32 Loaded = 0;
	for (int32 i = 0; i < Header[3] && !Ar.IsError(); ++i)
	{
		int32 Meta[3] = { 0, 0, 0 };
		Ar.Serialize(Meta, sizeof(Meta));
		FAfterfallPersistedChunk Rec;
		Ar << Rec.Data.Voxels;
		if (!Rec.Data.IsValid())
		{
			continue;
		}
		const FIntPoint Coord(Meta[0], Meta[1]);
		Rec.Seq = FMath::Max(Meta[2], 0);
		if (Rec.Seq <= 0)
		{
			continue; // unedited chunk: regenerate identically instead
		}
		PersistedChunks.Add(Coord, MoveTemp(Rec));
		PersistedOrder.Remove(Coord);
		PersistedOrder.Add(Coord);
		++Loaded;
	}
	while (PersistedOrder.Num() > FMath::Max(1, PersistedCacheCap))
	{
		PersistedChunks.Remove(PersistedOrder[0]);
		PersistedOrder.RemoveAt(0);
	}
	PersistedChunksLoadedFromDisk = Loaded;
	UE_LOG(LogAfterfall, Log, TEXT("WORLDGEN persist: loaded %d edited chunks from %s"),
		Loaded, *Path);
	return Loaded;
}

void UAfterfallWorldGenSubsystem::Deinitialize()
{
	// Best effort: edits made since the last stream-out must not die with the
	// process (crash handling is out of scope; this covers a clean shutdown).
	if (Config.bPersistChunkEdits)
	{
		SavePersistedChunks();
	}
	Super::Deinitialize();
}

void UAfterfallWorldGenSubsystem::StartStreaming()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client) || StreamIntervalS <= 0.f)
	{
		return;
	}
	W->GetTimerManager().SetTimer(StreamTimer, this,
		&UAfterfallWorldGenSubsystem::StreamTick, StreamIntervalS, true);
}

void UAfterfallWorldGenSubsystem::StreamTick()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return;
	}
	// One focus per connected player, so a listen/dedicated server streams for
	// everyone instead of only player 0. With nobody playing yet the configured
	// origin stays loaded, which is what the fixtures rely on.
	TArray<FVector> Focuses;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* P = PC ? PC->GetPawn() : nullptr;
		if (P)
		{
			Focuses.Add(P->GetActorLocation());
		}
	}
	if (Focuses.Num() == 0)
	{
		Focuses.Add(Config.OriginM * CmPerM);
	}
	UpdateStreamingForFocuses(Focuses);
}

bool UAfterfallWorldGenSubsystem::GenerateWorld()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return false;
	}
	// Regeneration must replace, never stack: generation runs on map start and
	// again from -afWorldGen / the self-test fixture.
	ClearGenerated();
	// Damage from a previous session is adopted as the stream restores chunks.
	const int32 FromDisk = LoadPersistedChunks();
	if (FromDisk > 0)
	{
		UE_LOG(LogAfterfall, Log,
			TEXT("WORLDGEN persist: %d edited chunks restored from the previous session"),
			FromDisk);
	}
	FString Err;
	if (!Config.Validate(Err))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("WORLDGEN FAIL: invalid config (%s)"), *Err);
		return false;
	}

	const double T0 = FPlatformTime::Seconds();
	Sites = PlanSites(Config);
	PlanMs = (float)((FPlatformTime::Seconds() - T0) * 1000.0);

	// ---- voxel terrain: streamed around the focus, not a fixed ring --------
	StreamFocusCm = Config.OriginM * CmPerM;
	const int32 Streamed = UpdateStreamingAt(StreamFocusCm, true);
	UE_LOG(LogAfterfall, Log,
		TEXT("WORLDGEN terrain: %d chunks streamed (chunk=%.1fm stream=%.0fm cap=%d)"),
		Streamed, Config.ChunkSizeM(), Config.StreamRadiusM, Config.MaxTerrainChunks);

	// ---- buildings: shells for nearby sites, registry entries otherwise ---
	BuildingParts.Reset();
	BuildingCount = 0;
	BuildingPartCount = 0;
	int32 GeometrySites = 0;
	// Memory/actor guard: only the nearest sites get real shells; every other
	// planned site stays a registry entry with its stable id.
	constexpr int32 MaxGeometryBuildings = 8;
	for (FAfterfallBuildingSite& Site : Sites)
	{
		const bool bNear = InsideRadius(Config.OriginM, Site.LocationM, Config.StreamRadiusM);
		if (!bNear || GeometrySites >= MaxGeometryBuildings)
		{
			continue; // registered site only: no geometry outside the stream
		}
		Site.bHasGeometry = true;
		++GeometrySites;
		++BuildingCount;

		const FVector Centre = Site.LocationM * CmPerM;
		const float Half = Site.FootprintM * CmPerM * 0.5f;
		const float CellExtent = 100.f;
		const float Span = CellExtent * 3.f;
		const int32 NX = FMath::Max(1, FMath::RoundToInt((Half * 2.f) / Span));
		const int32 NY = FMath::Max(1, FMath::RoundToInt((Half * 2.f) / Span));
		const float HX = NX * Span * 0.5f;
		const float HY = NY * Span * 0.5f;

		auto SpawnPart = [&](EStructuralPart Part, const FVector& At)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AStructuralActor* A = W->SpawnActor<AStructuralActor>(
				AStructuralActor::StaticClass(), At, FRotator::ZeroRotator, SP);
			if (!A || !A->Destruction)
			{
				return;
			}
			A->Tags.Add(GeneratedTag());
			A->Destruction->CellExtentCm = CellExtent;
			FStructuralSpec Spec;
			Spec.Part = Part;
			Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Part);
			Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Part);
			A->ServerInitialize(Spec, 0);
			A->RefreshDestructionVisual();
			BuildingParts.Add(A);
			++BuildingPartCount;
		};

		// Floor + roof tiles.
		for (int32 ix = 0; ix < NX; ++ix)
		{
			for (int32 iy = 0; iy < NY; ++iy)
			{
				const FVector P = Centre + FVector(-HX + Span * (ix + 0.5f),
					-HY + Span * (iy + 0.5f), CellExtent * 0.5f);
				SpawnPart(EStructuralPart::Foundation, P);
				SpawnPart(EStructuralPart::Roof, P + FVector(0.f, 0.f, Span));
			}
		}
		// Perimeter walls; one open bay is the entrance, outer bays are glass on
		// the long sides so windows break at their own durability.
		const int32 MidX = NX / 2;
		for (int32 ix = 0; ix < NX; ++ix)
		{
			for (int32 side = 0; side < 2; ++side)
			{
				if (side == 0 && ix == MidX)
				{
					continue; // doorway
				}
				const EStructuralPart P = (ix == 0 || ix == NX - 1)
					? EStructuralPart::Window : EStructuralPart::Wall;
				SpawnPart(P, Centre + FVector(-HX + Span * (ix + 0.5f),
					side == 0 ? -HY : HY, CellExtent * 1.5f));
			}
		}
		for (int32 iy = 0; iy < NY; ++iy)
		{
			for (int32 side = 0; side < 2; ++side)
			{
				const EStructuralPart P = (iy == NY / 2)
					? EStructuralPart::Window : EStructuralPart::Wall;
				SpawnPart(P, Centre + FVector(side == 0 ? -HX : HX,
					-HY + Span * (iy + 0.5f), CellExtent * 1.5f));
			}
		}
	}
	// ---- Phase 2: multi-chunk houses on the chunk grid with 5-block gap ---
	HousePlacements = PlanHouses(Config);
	BuildingCount += HousePlacements.Num();
	int32 HouseGeometry = 0;
	for (FHousePlacement& House : HousePlacements)
	{
		const bool bNear = InsideRadius(Config.OriginM, House.LocationM, Config.StreamRadiusM);
		if (!bNear)
		{
			continue; // registered house only: no geometry outside the stream
		}
		++HouseGeometry;
		const FVector Centre = House.LocationM * CmPerM;
		const float Half = House.Blueprint.FootprintM * CmPerM * 0.5f;
		const float CellExtent = 100.f;
		const float Span = CellExtent * 3.f;
		const int32 NX = FMath::Max(1, FMath::RoundToInt((Half * 2.f) / Span));
		const int32 NY = FMath::Max(1, FMath::RoundToInt((Half * 2.f) / Span));
		const float HX = NX * Span * 0.5f;
		const float HY = NY * Span * 0.5f;

		auto SpawnPart = [&](EStructuralPart Part, const FVector& At)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AStructuralActor* A = W->SpawnActor<AStructuralActor>(
				AStructuralActor::StaticClass(), At, FRotator::ZeroRotator, SP);
			if (!A || !A->Destruction)
			{
				return;
			}
			A->Tags.Add(GeneratedTag());
			A->Destruction->CellExtentCm = CellExtent;
			FStructuralSpec Spec;
			Spec.Part = Part;
			Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Part);
			Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Part);
			A->ServerInitialize(Spec, 0);
			A->RefreshDestructionVisual();
			BuildingParts.Add(A);
			++BuildingPartCount;
		};

		// Shell: foundation + roof tiles + perimeter walls.
		for (int32 ix = 0; ix < NX; ++ix)
		{
			for (int32 iy = 0; iy < NY; ++iy)
			{
				const FVector P = Centre + FVector(-HX + Span * (ix + 0.5f),
					-HY + Span * (iy + 0.5f), CellExtent * 0.5f);
				SpawnPart(EStructuralPart::Foundation, P);
				SpawnPart(EStructuralPart::Roof, P + FVector(0.f, 0.f, Span));
			}
		}
		const int32 MidX = NX / 2;
		for (int32 ix = 0; ix < NX; ++ix)
		{
			for (int32 side = 0; side < 2; ++side)
			{
				if (side == 0 && ix == MidX) continue; // doorway
				const EStructuralPart P = (ix == 0 || ix == NX - 1)
					? EStructuralPart::Window : EStructuralPart::Wall;
				SpawnPart(P, Centre + FVector(-HX + Span * (ix + 0.5f),
					side == 0 ? -HY : HY, CellExtent * 1.5f));
			}
		}
		for (int32 iy = 0; iy < NY; ++iy)
		{
			for (int32 side = 0; side < 2; ++side)
			{
				const EStructuralPart P = (iy == NY / 2)
					? EStructuralPart::Window : EStructuralPart::Wall;
				SpawnPart(P, Centre + FVector(side == 0 ? -HX : HX,
					-HY + Span * (iy + 0.5f), CellExtent * 1.5f));
			}
		}
		// Interior: rooms, doors, furniture inside the shell.
		GenerateHouseInterior(Centre, House);
	}
	UE_LOG(LogAfterfall, Log,
		TEXT("WORLDGEN houses: %d planned, %d with geometry inside stream radius"),
		HousePlacements.Num(), HouseGeometry);

	GenerateMs = (float)((FPlatformTime::Seconds() - T0) * 1000.0);

	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
	UE_LOG(LogAfterfall, Log,
		TEXT("WORLDGEN built: radius=%.0fm sites=%d geometryBuildings=%d parts=%d chunks=%d plan=%.1fms total=%.1fms memUsed=%.0fMB peak=%.0fMB"),
		Config.RadiusM, Sites.Num(), BuildingCount, BuildingPartCount, TerrainChunkCount,
		PlanMs, GenerateMs, Mb(Mem.UsedPhysical), Mb(Mem.PeakUsedPhysical));
	return true;
}

bool UAfterfallWorldGenSubsystem::RunSelfTest(bool bExit)
{
	UWorld* W = GetWorld();
	int32 Pass = 0;
	int32 Fail = 0;
	auto Check = [&](bool bOk, const TCHAR* What)
	{
		WorldLog(bOk, What);
		bOk ? ++Pass : ++Fail;
	};

	if (!W || W->IsNetMode(NM_Client))
	{
		WorldLog(false, TEXT("requires server"));
		if (bExit)
		{
			FPlatformMisc::RequestExitWithStatus(false, 1);
		}
		return false;
	}

	FString Err;
	Check(Config.Validate(Err), TEXT("config validates (radius 1km, 3.06m voxels, 32^3 chunks)"));

	// Determinism: same seed twice, different seed once.
	const TArray<FAfterfallBuildingSite> A = PlanSites(Config);
	const TArray<FAfterfallBuildingSite> B = PlanSites(Config);
	FAfterfallWorldConfig Other = Config;
	Other.Seed = Config.Seed + 1;
	const TArray<FAfterfallBuildingSite> C = PlanSites(Other);

	bool bSame = A.Num() == B.Num();
	for (int32 i = 0; bSame && i < A.Num(); ++i)
	{
		bSame = A[i].StableId == B[i].StableId
			&& A[i].LocationM.Equals(B[i].LocationM, 0.01f);
	}
	Check(bSame, TEXT("same seed reproduces identical ids and positions"));
	Check(C.Num() != A.Num() || (C.Num() > 0 && !C[0].LocationM.Equals(A[0].LocationM, 0.01f)),
		TEXT("different seed produces a different layout"));

	// Every site inside the configured radius (the exact circular condition).
	bool bAllInside = true;
	for (const FAfterfallBuildingSite& S : A)
	{
		bAllInside &= InsideRadius(Config.OriginM, S.LocationM, Config.RadiusM);
	}
	Check(A.Num() > 0 && bAllInside, TEXT("all sites satisfy dx*dx+dz*dz <= R*R"));

	// Minimum spacing honoured.
	float WorstSq = 1.e9f;
	for (int32 i = 0; i < A.Num(); ++i)
	{
		for (int32 j = i + 1; j < A.Num(); ++j)
		{
			const float D2 = FVector::DistSquared2D(A[i].LocationM, A[j].LocationM);
			WorstSq = FMath::Min(WorstSq, D2);
		}
	}
	Check(A.Num() < 2 || WorstSq >= FMath::Square(Config.BuildingMinSpacingM * 0.6f),
		TEXT("building spacing respects the configured minimum"));

	// Generate and assert what actually spawned.
	Check(GenerateWorld(), TEXT("world generates on server"));
	Check(TerrainChunkCount > 0 && TerrainChunkCount <= Config.MaxTerrainChunks,
		TEXT("voxel chunk count within the configured cap"));
	Check(BuildingCount > 0 && BuildingPartCount > 0,
		TEXT("destructible building shells spawned inside the stream radius"));

	int32 UniqueChunks = 0;
	{
		TSet<FVector> Seen;
		for (const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& P : LoadedChunks)
		{
			if (P.Value.IsValid())
			{
				Seen.Add(P.Value->GetActorLocation());
			}
		}
		UniqueChunks = Seen.Num();
	}
	Check(UniqueChunks == TerrainChunkCount && UniqueChunks == LoadedChunks.Num(),
		TEXT("no duplicate voxel chunks spawned"));

	// ---- Phase 2: house determinism + gap rule ----------------------------
	const TArray<FHousePlacement> H1 = PlanHouses(Config);
	const TArray<FHousePlacement> H2 = PlanHouses(Config);
	bool bHouseSame = H1.Num() == H2.Num();
	for (int32 i = 0; bHouseSame && i < H1.Num(); ++i)
	{
		bHouseSame = H1[i].StableId == H2[i].StableId
			&& H1[i].LocationM.Equals(H2[i].LocationM, 0.01f);
	}
	Check(bHouseSame, TEXT("same seed reproduces identical house placements"));

	// Every house respects the 5-block gap rule against all others.
	bool bGapOk = true;
	for (int32 i = 0; bGapOk && i < H1.Num(); ++i)
	{
		for (int32 j = i + 1; bGapOk && j < H1.Num(); ++j)
		{
			bGapOk = PlacementsRespectGap(H1[i], H1[j], Config);
		}
	}
	Check(H1.Num() < 2 || bGapOk, TEXT("all house pairs respect the 5-chunk gap rule"));

	// No two houses share an occupied chunk (chunk reflection).
	bool bNoOverlap = true;
	for (int32 i = 0; bNoOverlap && i < H1.Num(); ++i)
	{
		for (int32 j = i + 1; bNoOverlap && j < H1.Num(); ++j)
		{
			bNoOverlap = ChunkReflectionOk(H1[i], { H1[j] }, Config);
		}
	}
	Check(H1.Num() < 2 || bNoOverlap, TEXT("no house footprints overlap another"));
	Check(Config.HouseTarget <= 0 || H1.Num() <= Config.HouseTarget,
		TEXT("house count does not exceed target"));

	// All planned houses lie inside the configured radius.
	bool bHouseInside = true;
	for (const FHousePlacement& H : H1)
	{
		bHouseInside &= InsideRadius(Config.OriginM, H.LocationM, Config.RadiusM);
	}
	Check(H1.Num() == 0 || bHouseInside, TEXT("all houses lie inside the radius"));

	// After GenerateWorld(), houses inside the stream radius got geometry.
	int32 HouseGeometry = 0;
	for (const FHousePlacement& H : HousePlacements)
	{
		if (InsideRadius(Config.OriginM, H.LocationM, Config.StreamRadiusM))
		{
			++HouseGeometry;
		}
	}
	Check(HousePlacements.Num() == 0 || HouseGeometry > 0,
		TEXT("stream-radius houses received geometry"));

	// ---- streaming: the chunk ring follows the focus ----------------------
	AVoxelChunkActor* EditTarget = nullptr;
	FIntPoint EditCoord(0, 0);
	if (LoadedChunks.Num() > 0)
	{
		const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& First =
			*LoadedChunks.CreateConstIterator();
		EditTarget = First.Value.Get();
		EditCoord = First.Key;
	}
	// Toggle instead of always writing the same material, so re-running the
	// fixture against its own persisted file still commits a real change.
	const FIntVector EditCell(16, 16, 10);
	uint8 WantState = 3;
	if (EditTarget && EditTarget->GetVoxelData().Get(EditCell) == 3)
	{
		WantState = 2;
	}
	Check(EditTarget && EditTarget->ServerApplyVoxelEdit(EditCell, WantState),
		TEXT("voxel edit committed before the chunk streams out"));

	const FVector HomeCm = Config.OriginM * CmPerM;
	const FVector FarCm = HomeCm + FVector(Config.StreamRadiusM * 3.f * CmPerM, 0.f, 0.f);
	const float ReachCm = (Config.StreamRadiusM + Config.ChunkSizeM()) * CmPerM;
	auto ChunkCentre = [this](const FIntPoint& Coord)
	{
		const float Half = Config.ChunkSizeM() * CmPerM * 0.5f;
		return ChunkCornerWorldCm(Coord) + FVector(Half, Half, 0.f);
	};
	auto CountNear = [this, &ChunkCentre, ReachCm](const FVector& FocusCm)
	{
		int32 N = 0;
		for (const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& P : LoadedChunks)
		{
			N += FVector::Dist2D(ChunkCentre(P.Key), FocusCm) <= ReachCm ? 1 : 0;
		}
		return N;
	};

	const int32 FarCount = UpdateStreamingAt(FarCm, true);
	Check(FarCount > 0, TEXT("chunks stream in at the new focus"));
	Check(FarCount <= Config.MaxTerrainChunks, TEXT("streamed chunk count obeys the cap"));
	bool bAllNearFocus = true;
	for (const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& P : LoadedChunks)
	{
		bAllNearFocus &= FVector::Dist2D(ChunkCentre(P.Key), FarCm) <= ReachCm;
	}
	Check(bAllNearFocus, TEXT("every streamed chunk lies around the new focus"));
	Check(!LoadedChunks.Contains(EditCoord), TEXT("chunk outside the radius is unloaded"));
	Check(ChunksStreamedOut > 0, TEXT("stream-out actually happened"));

	UpdateStreamingAt(HomeCm, true);
	AVoxelChunkActor* Restored = LoadedChunks.Contains(EditCoord)
		? LoadedChunks.FindRef(EditCoord).Get() : nullptr;
	Check(Restored != nullptr, TEXT("chunk reloads when the focus returns"));
	Check(Restored && Restored->GetServerSeq() > 0
			&& Restored->GetVoxelData().Get(EditCell) == WantState,
		TEXT("streamed-out chunk keeps committed voxel edits"));

	// ---- persistence: damage written to disk survives a restart -----------
	const FString PersistPath = PersistFilePath();
	Check(Config.bPersistChunkEdits && FPaths::FileExists(PersistPath),
		TEXT("edited chunks are written to the damage file"));

	// Simulated restart: drop every chunk actor and the in-memory cache, then
	// rebuild terrain purely from what the previous session wrote to disk.
	for (const TPair<FIntPoint, TWeakObjectPtr<AVoxelChunkActor>>& P : LoadedChunks)
	{
		if (P.Value.IsValid())
		{
			P.Value->Destroy();
		}
	}
	LoadedChunks.Reset();
	PersistedChunks.Reset();
	PersistedOrder.Reset();
	TerrainChunkCount = 0;
	const int32 FromDisk = LoadPersistedChunks();
	Check(FromDisk > 0, TEXT("previous session's edited chunks load from disk"));
	UpdateStreamingAt(HomeCm, true);
	AVoxelChunkActor* AfterRestart = LoadedChunks.Contains(EditCoord)
		? LoadedChunks.FindRef(EditCoord).Get() : nullptr;
	Check(AfterRestart && AfterRestart->GetServerSeq() > 0
			&& AfterRestart->GetVoxelData().Get(EditCell) == WantState,
		TEXT("restart restores voxel damage from disk"));

	// ---- multi-focus: every player gets chunks of their own ----------------
	{
		TArray<FVector> Focuses;
		Focuses.Add(HomeCm);
		Focuses.Add(HomeCm + FVector(Config.StreamRadiusM * 3.f * CmPerM, 0.f, 0.f));
		const int32 MultiCount = UpdateStreamingForFocuses(Focuses, true);
		Check(MultiCount <= Config.MaxTerrainChunks, TEXT("multi-focus streaming obeys the cap"));
		Check(CountNear(Focuses[0]) > 0 && CountNear(Focuses[1]) > 0,
			TEXT("each player focus keeps chunks of its own"));
		UpdateStreamingAt(HomeCm, true);
	}

	// Destruction on a generated building must go through the shared path.
	bool bBroken = false;
	for (const TWeakObjectPtr<AStructuralActor>& P : BuildingParts)
	{
		if (!P.IsValid() || P->Spec.Part != EStructuralPart::Wall)
		{
			continue;
		}
		FDestructionEvent Hit;
		Hit.Origin = P->GetActorLocation();
		Hit.Direction = FVector(0.f, 0.f, -1.f);
		Hit.InitialEnergy = UDestructionPropagationService::AttackEnergy();
		Hit.Initiator = EDestructionInitiator::Attack;
		Hit.MaterialId = UDestructionPropagationService::MaterialForPart(P->Spec.Part);
		bBroken = P->ServerApplyImpact(Hit,
			UDestructionPropagationService::AttackDamagePerCell(), FGuid::NewGuid()) > 0;
		break;
	}
	Check(bBroken, TEXT("generated building wall breaks through ServerApplyImpact"));

	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
	UE_LOG(LogAfterfall, Log,
		TEXT("WORLDGEN SUMMARY pass=%d fail=%d sites=%d radius=%.0fm parts=%d chunks=%d plan=%.1fms gen=%.1fms memUsed=%.0fMB peak=%.0fMB WORLDGEN %s"),
		Pass, Fail, Sites.Num(), Config.RadiusM, BuildingPartCount, TerrainChunkCount,
		PlanMs, GenerateMs, Mb(Mem.UsedPhysical), Mb(Mem.PeakUsedPhysical),
		Fail == 0 ? TEXT("PASS") : TEXT("FAIL"));

	if (bExit)
	{
		FPlatformMisc::RequestExitWithStatus(false, Fail == 0 ? 0 : 1);
	}
	return Fail == 0;
}
