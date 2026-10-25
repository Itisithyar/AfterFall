#pragma once
#include "CoreMinimal.h"
#include "VoxelBasis.h"

struct FVoxelChunk
{
	static constexpr int32 Size = 32;
	uint8 Cells[Size*Size*Size] = {};
	bool bDirty = true;
	int32 Revision = 0;
	bool Get(int32 x, int32 y, int32 z) const
	{
		if (x<0||x>=Size||y<0||y>=Size||z<0||z>=Size) return false;
		return Cells[x + y*Size + z*Size*Size] != 0;
	}
	void Set(int32 x, int32 y, int32 z, uint8 v)
	{
		if (x<0||x>=Size||y<0||y>=Size||z<0||z>=Size) return;
		Cells[x + y*Size + z*Size*Size] = v;
		bDirty = true;
		++Revision;
	}
};