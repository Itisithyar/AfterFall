#pragma once
#include "CoreMinimal.h"

struct FRhombusQuad
{
	FVector V0,V1,V2,V3;
	FVector2D UV0,UV1,UV2,UV3;
	FVector Normal;
	void AppendToMeshBuffers(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UVs, int32& IndexOffset) const
	{
		int32 b = IndexOffset;
		Verts.Add(V0); Verts.Add(V1); Verts.Add(V2); Verts.Add(V3);
		Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal);
		UVs.Add(UV0); UVs.Add(UV1); UVs.Add(UV2); UVs.Add(UV3);
		Tris.Add(b+0); Tris.Add(b+1); Tris.Add(b+2);
		Tris.Add(b+0); Tris.Add(b+2); Tris.Add(b+3);
		IndexOffset += 4;
	}
};