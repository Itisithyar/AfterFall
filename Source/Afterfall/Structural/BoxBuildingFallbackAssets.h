// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: BEAM Free (Priority 4 assets) — procedural-fallback asset mapping.
// HEADER-ONLY, no UCLASS/USTRUCT/UHT: pure inline FSoftObjectPath getters so
// this file cannot break the module build. It duplicates NO destruction logic;
// the runtime caller (FLINT's EvaluateImpact -> ServerApplyImpact wiring) consumes
// these paths when spawning instanced intact walls and simulated detached blocks.
// Keys are FName material ids matching UBoxBuildingLib::MaterialId() strings
// ("Drywall","Wood","Brick","Concrete","Glass","Metal") — deliberately NOT
// #including BoxBuilding.h so renames there cannot break this file.
#pragma once

#include "CoreMinimal.h"

namespace BoxFallbackAssets
{
	/** Intact wall blocks (instanced/merged by the caller). 100 cm prototype cube. */
	inline FSoftObjectPath IntactBlockMesh()
	{
		return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Meshes/SM_Cube"));
	}

	/** Detached/simulated blocks and rubble chunks (chamfer reads as broken). */
	inline FSoftObjectPath DetachedBlockMesh()
	{
		return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Meshes/SM_ChamferCube"));
	}

	/** Door/window opening dressing for FBoxOpening rects. */
	inline FSoftObjectPath DoorFrameEdgeMesh()
	{
		return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Interactable/Door/Meshes/SM_DoorFrame_Edge"));
	}
	inline FSoftObjectPath DoorFrameCornerMesh()
	{
		return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Interactable/Door/Meshes/SM_DoorFrame_Corner"));
	}
	inline FSoftObjectPath DoorMesh()
	{
		return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Interactable/Door/Meshes/SM_Door"));
	}

	/** Per-material look (prototype set; distinct per material until Fab PBR sets arrive).
	 *  Empty path = no suitable in-project material; caller keeps prior material. */
	inline FSoftObjectPath MaterialForId(FName MaterialId)
	{
		if (MaterialId == TEXT("Drywall"))  return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray"));
		if (MaterialId == TEXT("Wood"))     return FSoftObjectPath(TEXT("/Game/FirstPerson/MI_FirstPersonColorway"));
		if (MaterialId == TEXT("Brick"))    return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_Round"));
		if (MaterialId == TEXT("Concrete")) return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark"));
		if (MaterialId == TEXT("Metal"))    return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02"));
		// FIX-D: explicit glass stand-in (flat pale look) until a translucent pane
		// material arrives via Fab. Deliberately distinct from every other slot.
		if (MaterialId == TEXT("Glass"))    return FSoftObjectPath(TEXT("/Game/LevelPrototyping/Materials/M_FlatCol"));
		return FSoftObjectPath();
	}

	/** Prototype meshes are 100 cm; profile default CubeSizeCm is 50. */
	inline float BlockMeshScale(float CubeSizeCm) { return CubeSizeCm / 100.f; }

	/** Recommended caps for the runtime caller (it owns enforcement). */
	constexpr int32 MaxSimulatedDebrisBlocks = 24;
	constexpr int32 MaxActiveCollapseWaves = 4;
} // namespace BoxFallbackAssets
