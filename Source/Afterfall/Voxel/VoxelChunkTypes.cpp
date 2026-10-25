// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — chunk data, ordered edit application, material palette.
#include "Voxel/VoxelChunkTypes.h"

FLinearColor AfterfallVoxel::MaterialColor(uint8 State)
{
	switch (State)
	{
	case 1: return FLinearColor(0.52f, 0.52f, 0.55f); // stone
	case 2: return FLinearColor(0.55f, 0.36f, 0.18f); // wood
	case 3: return FLinearColor(0.62f, 0.64f, 0.68f); // metal
	case 4: return FLinearColor(0.45f, 0.30f, 0.22f); // scrap
	default: return FLinearColor::Black;
	}
}

int32 FVoxelChunkData::VoxelIndex(const FIntVector& Local)
{
	return Local.X + AfterfallVoxel::ChunkSize * (Local.Y + AfterfallVoxel::ChunkSize * Local.Z);
}

bool FVoxelChunkData::IsValidCoord(const FIntVector& Local)
{
	return Local.X >= 0 && Local.X < AfterfallVoxel::ChunkSize
		&& Local.Y >= 0 && Local.Y < AfterfallVoxel::ChunkSize
		&& Local.Z >= 0 && Local.Z < AfterfallVoxel::ChunkSize;
}

void FVoxelChunkData::Reset()
{
	Voxels.Reset();
	Voxels.SetNumZeroed(AfterfallVoxel::ChunkVoxelCount);
}

void FVoxelChunkData::InitializeDeterministic()
{
	Reset();
	for (int32 Z = 0; Z < 8; ++Z)
	{
		for (int32 Y = 0; Y < AfterfallVoxel::ChunkSize; ++Y)
		{
			for (int32 X = 0; X < AfterfallVoxel::ChunkSize; ++X)
			{
				Voxels[VoxelIndex(FIntVector(X, Y, Z))] = 1; // stone
			}
		}
	}
}

bool FVoxelChunkData::Set(const FIntVector& Local, uint8 State)
{
	if (!IsValidCoord(Local) || State > AfterfallVoxel::MaxMaterialId)
	{
		return false;
	}
	if (!IsValid())
	{
		return false;
	}
	Voxels[VoxelIndex(Local)] = State;
	return true;
}

uint8 FVoxelChunkData::Get(const FIntVector& Local) const
{
	// Out-of-chunk samples read as empty: this chunk has no neighbours in the
	// single-chunk slice, so border faces are visible.
	if (!IsValidCoord(Local) || !IsValid())
	{
		return 0;
	}
	return Voxels[VoxelIndex(Local)];
}

int32 FVoxelChunkData::CountSolid() const
{
	int32 N = 0;
	for (uint8 V : Voxels)
	{
		if (V != 0)
		{
			++N;
		}
	}
	return N;
}

int32 FVoxelChunkData::CountMaterial(uint8 State) const
{
	int32 N = 0;
	for (uint8 V : Voxels)
	{
		if (V == State)
		{
			++N;
		}
	}
	return N;
}

bool AfterfallVoxel::ApplyEdit(FVoxelChunkData& Data, const FVoxelEdit& Edit, int32& InOutLastSeq)
{
	// Duplicate (replay/retry) and stale (already superseded) edits are no-ops.
	if (Edit.Seq <= InOutLastSeq)
	{
		return false;
	}
	if (!FVoxelChunkData::IsValidCoord(Edit.Coord) || Edit.State > AfterfallVoxel::MaxMaterialId)
	{
		// A malformed edit cannot be applied, but the sequence must still
		// advance or every later edit would look like a gap.
		InOutLastSeq = Edit.Seq;
		return false;
	}
	const bool bChanged = Data.Get(Edit.Coord) != Edit.State;
	Data.Set(Edit.Coord, Edit.State);
	InOutLastSeq = Edit.Seq;
	return bChanged;
}

bool AfterfallVoxel::IsGap(int32 Seq, int32 LastSeq)
{
	return Seq > LastSeq + 1;
}
