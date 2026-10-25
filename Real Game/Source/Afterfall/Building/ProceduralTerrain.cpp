#include "ProceduralTerrain.h"
#include "PCGComponent.h"
#include "Afterfall.h"
#include "Net/UnrealNetwork.h"

AProceduralTerrain::AProceduralTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(SceneRoot);
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    PlacementPCG = CreateDefaultSubobject<UPCGComponent>(TEXT("TerrainPlacementPCG"));
    PlacementPCG->InputType = EPCGComponentInput::Actor;
    PlacementPCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	TerrainMesh->bUseAsyncCooking = true;
}

void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();
	RebuildTerrain();
}

void AProceduralTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProceduralTerrain, TileCountX);
	DOREPLIFETIME(AProceduralTerrain, TileCountY);
	DOREPLIFETIME(AProceduralTerrain, TileSize);
	DOREPLIFETIME(AProceduralTerrain, HeightScale);
	DOREPLIFETIME(AProceduralTerrain, RandomSeed);
	DOREPLIFETIME(AProceduralTerrain, bFlatMode);
}

void AProceduralTerrain::OnRep_TerrainConfig()
{
	RebuildTerrain();
}

void AProceduralTerrain::RebuildTerrain()
{
	if (!TerrainMesh)
	{
		return;
	}
	const int32 NX = FMath::Clamp(TileCountX, 8, 300) + 1;
	const int32 NY = FMath::Clamp(TileCountY, 8, 300) + 1;
	const int32 CellsX = NX - 1;
	const int32 CellsY = NY - 1;
	const float SafeTileSize = FMath::Max(10.f, TileSize);
	const float TotalX = CellsX * SafeTileSize;
	const float TotalY = CellsY * SafeTileSize;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	Vertices.SetNum(NX * NY);
	Normals.SetNum(NX * NY);
	UVs.SetNum(NX * NY);
	Triangles.Reserve(CellsX * CellsY * 6);
	const float SeedOffset = static_cast<float>(RandomSeed % 100003) * 0.001f;
	const float EdgeFade = FMath::Max(SafeTileSize * 2.f, 15000.f);
	const float SpawnPadRadius = FMath::Min(12000.f, FMath::Min(TotalX, TotalY) * 0.15f);
	for (int32 Y = 0; Y < NY; ++Y)
	{
		for (int32 X = 0; X < NX; ++X)
		{
			const float PX = X * SafeTileSize - TotalX * 0.5f;
			const float PY = Y * SafeTileSize - TotalY * 0.5f;
			const float N1 = FMath::PerlinNoise2D(FVector2D(PX * 0.000035f + SeedOffset, PY * 0.000035f - SeedOffset));
			const float N2 = FMath::PerlinNoise2D(FVector2D(PX * 0.00011f - SeedOffset, PY * 0.00011f + SeedOffset * 0.37f));
			const float N3 = FMath::PerlinNoise2D(FVector2D(PX * 0.00032f + SeedOffset * 0.13f, PY * 0.00032f));
			const float EdgeDistance = FMath::Min(TotalX * 0.5f - FMath::Abs(PX), TotalY * 0.5f - FMath::Abs(PY));
			const float EdgeT = FMath::Clamp(EdgeDistance / EdgeFade, 0.f, 1.f);
			const float EdgeWeight = EdgeT * EdgeT * (3.f - 2.f * EdgeT);
			const float SpawnDistance = FVector2D(PX, PY).Length();
			const float PadT = FMath::Clamp((SpawnDistance - SpawnPadRadius) / FMath::Max(SpawnPadRadius, 1.f), 0.f, 1.f);
			const float PadWeight = PadT * PadT * (3.f - 2.f * PadT);
			// Seeded, low-relief terrain with a level spawn apron. Height is
			// bounded above the collision underlay so the playable surface never
			// dips through it. Impact and deformation equations remain in the
			// shared SDF object pipeline rather than being duplicated here.
			const float Relief = FMath::Max(0.f, (N1 * 0.58f + N2 * 0.29f + N3 * 0.13f) * HeightScale);
			const float Height = 25.f + (bFlatMode ? 0.f : Relief * EdgeWeight * PadWeight);
			const int32 Index = Y * NX + X;
			Vertices[Index] = FVector(PX, PY, Height);
			UVs[Index] = FVector2D(static_cast<float>(X) / CellsX, static_cast<float>(Y) / CellsY);
		}
	}
	for (int32 Y = 0; Y < NY; ++Y)
	{
		for (int32 X = 0; X < NX; ++X)
		{
			const int32 Left = Y * NX + FMath::Max(0, X - 1);
			const int32 Right = Y * NX + FMath::Min(NX - 1, X + 1);
			const int32 Down = FMath::Max(0, Y - 1) * NX + X;
			const int32 Up = FMath::Min(NY - 1, Y + 1) * NX + X;
			const float DX = FMath::Max(SafeTileSize, (X == 0 || X == NX - 1) ? SafeTileSize : SafeTileSize * 2.f);
			const float DY = FMath::Max(SafeTileSize, (Y == 0 || Y == NY - 1) ? SafeTileSize : SafeTileSize * 2.f);
			const float DZDX = (Vertices[Right].Z - Vertices[Left].Z) / DX;
			const float DZDY = (Vertices[Up].Z - Vertices[Down].Z) / DY;
			Normals[Y * NX + X] = FVector(-DZDX, -DZDY, 1.f).GetSafeNormal();
		}
	}
	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const int32 I0 = Y * NX + X;
			const int32 I1 = I0 + 1;
			const int32 I2 = I0 + NX;
			const int32 I3 = I2 + 1;
			Triangles.Add(I0);
			Triangles.Add(I1);
			Triangles.Add(I3);
			Triangles.Add(I0);
			Triangles.Add(I3);
			Triangles.Add(I2);
		}
	}
	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainMesh->SetGenerateOverlapEvents(false);
	// Generate placement only after the procedural surface exists.
	if (PlacementPCG && PlacementGraph)
	{
		PlacementPCG->SetGraphLocal(PlacementGraph);
		PlacementPCG->GenerateLocal(true);
	}
	UE_LOG(LogAfterfall, Log, TEXT("Generated terrain grid %dx%d, %.0fm x %.0fm, seed=%d"),
		NX, NY, TotalX / 100.f, TotalY / 100.f, RandomSeed);
}
