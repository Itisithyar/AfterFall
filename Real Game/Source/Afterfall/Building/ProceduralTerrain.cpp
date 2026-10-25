#include "ProceduralTerrain.h"
#include "Afterfall.h"
#include "Geometry/GeometryTypes.h"

AProceduralTerrain::AProceduralTerrain() { PrimaryActorTick.bCanEverTick = false; }

void AProceduralTerrain::BeginPlay() { Super::BeginPlay(); RebuildTerrain(); }

void AProceduralTerrain::RebuildTerrain()
{
	int32 NX = TileCountX + 1, NY = TileCountY + 1;
	TArray<FVector> Verts; Verts.SetNum(NX*NY);
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
	}
	TArray<FRhombusQuad> Quads;
	for (int32 y = 0; y < TileCountY; y++)
	for (int32 x = 0; x < TileCountX; x++)
	{
		int32 i0 = y*NX+x, i1 = y*NX+x+1, i2 = (y+1)*NX+x, i3 = (y+1)*NX+x+1;
		FRhombusQuad Q;
		Q.V0 = Verts[i0];
		Q.V1 = Verts[i1] + FVector(RhombusShear*TileSize,0,0);
		Q.V2 = Verts[i3] + FVector(RhombusShear*TileSize,0,0);
		Q.V3 = Verts[i2];
		Q.UV0 = FVector2D((float)x/TileCountX, (float)y/TileCountY);
		Q.UV1 = FVector2D((float)(x+1)/TileCountX, (float)y/TileCountY);
		Q.UV2 = FVector2D((float)(x+1)/TileCountX, (float)(y+1)/TileCountY);
		Q.UV3 = FVector2D((float)x/TileCountX, (float)(y+1)/TileCountY);
		Q.Normal = FVector::UpVector;
		Quads.Add(Q);
	}
	TArray<FVector> OutVerts; TArray<int32> OutTris; TArray<FVector> OutNorms; TArray<FVector2D> OutUVs; int32 Off=0;
	for (const FRhombusQuad& Q : Quads) Q.AppendToMeshBuffers(OutVerts, OutTris, OutNorms, OutUVs, Off);
	UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
	PMC->RegisterComponent();
	PMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PMC->CreateMeshSection_LinearColor(0, OutVerts, OutTris, OutNorms, OutUVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	if (TerrainMaterial) PMC->SetMaterial(0, TerrainMaterial);
	PMC->bUseComplexAsSimpleCollision = true;
	PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}