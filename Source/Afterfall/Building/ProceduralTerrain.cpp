#include "ProceduralTerrain.h"
#include "Afterfall.h"

AProceduralTerrain::AProceduralTerrain() { PrimaryActorTick.bCanEverTick = false; }

void AProceduralTerrain::BeginPlay() { Super::BeginPlay(); RebuildTerrain(); }

void AProceduralTerrain::RebuildTerrain()
{
	int32 NX = TileCountX + 1, NY = TileCountY + 1;
	TArray<FVector> Verts; Verts.SetNum(NX*NY);
	TArray<FVector2D> UVs; UVs.SetNum(NX*NY);
	float TotalX = TileCountX * TileSize, TotalY = TileCountY * TileSize;
	for (int32 y = 0; y < NY; y++)
	for (int32 x = 0; x < NX; x++)
	{
		float px = x * TileSize - TotalX*0.5f;
		float py = y * TileSize - TotalY*0.5f;
		float h = 0.f;
		if (!bFlatMode)
		{
			float n1 = FMath::PerlinNoise2D(FVector2D((x+RandomSeed)*0.05f, (y+RandomSeed)*0.05f));
			float n2 = FMath::PerlinNoise2D(FVector2D((x+RandomSeed)*0.1f, (y+RandomSeed)*0.1f));
			h = (n1 + 0.5f*n2) * HeightScale;
		}
		Verts[y*NX+x] = FVector(px, py, h);
		UVs[y*NX+x] = FVector2D(px/TotalX + 0.5f, py/TotalY + 0.5f);
	}
	TArray<int32> Tris;
	for (int32 y = 0; y < TileCountY; y++)
	for (int32 x = 0; x < TileCountX; x++)
	{
		int32 i0 = y*NX+x, i1 = y*NX+x+1, i2 = (y+1)*NX+x, i3 = (y+1)*NX+x+1;
		// merge check on the two tris
		Afterfall::Math::CanMergeTriangles(Verts[i0],Verts[i1],Verts[i2], Verts[i1],Verts[i3],Verts[i2]);
		Tris.Add(i0); Tris.Add(i1); Tris.Add(i2);
		Tris.Add(i1); Tris.Add(i3); Tris.Add(i2);
	}
	// normals
	TArray<FVector> Norms; Norms.SetNum(NX*NY);
	for (int32 i = 0; i < Norms.Num(); i++) Norms[i] = FVector::UpVector;
	UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
	PMC->RegisterComponent();
	PMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PMC->CreateMeshSection_LinearColor(0, Verts, Tris, Norms, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	if (TerrainMaterial) PMC->SetMaterial(0, TerrainMaterial);
	PMC->bUseComplexAsSimpleCollision = true;
	PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}