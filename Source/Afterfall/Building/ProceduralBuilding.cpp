#include "ProceduralBuilding.h"
#include "Afterfall.h"

AProceduralBuilding::AProceduralBuilding() { PrimaryActorTick.bCanEverTick = false; }

void AProceduralBuilding::BeginPlay() { Super::BeginPlay(); RebuildBuilding(); }

void AProceduralBuilding::RebuildBuilding()
{
	// clear existing
	TArray<UActorComponent*> Comps; GetComponents(Comps);
	for (UActorComponent* C : Comps) if (UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C)) PMC->DestroyComponent();
	for (int32 f = 0; f < FloorCount; f++) BuildFloor(f);
}

void AProceduralBuilding::BuildFloor(int32 FloorIndex)
{
	float z = FloorIndex * FloorHeight;
	float hx = FootprintSize.X * 0.5f, hy = FootprintSize.Y * 0.5f;
	// 4 walls as simple quads split into two triangles; merge where coplanar (trivially)
	struct Face { FVector a,b,c,d; FString tag; };
	TArray<Face> Faces;
	Faces.Add({ FVector(-hx,-hy,z), FVector(hx,-hy,z), FVector(hx,-hy,z+FloorHeight), FVector(-hx,-hy,z+FloorHeight), TEXT("South") });
	Faces.Add({ FVector(hx,-hy,z), FVector(hx,hy,z), FVector(hx,hy,z+FloorHeight), FVector(hx,-hy,z+FloorHeight), TEXT("East") });
	Faces.Add({ FVector(hx,hy,z), FVector(-hx,hy,z), FVector(-hx,hy,z+FloorHeight), FVector(hx,hy,z+FloorHeight), TEXT("North") });
	Faces.Add({ FVector(-hx,hy,z), FVector(-hx,-hy,z), FVector(-hx,-hy,z+FloorHeight), FVector(-hx,hy,z+FloorHeight), TEXT("West") });
	// floor slab
	Faces.Add({ FVector(-hx,-hy,z), FVector(hx,-hy,z), FVector(hx,hy,z), FVector(-hx,hy,z), TEXT("Floor") });
	for (const Face& F : Faces)
	{
		TArray<FVector> TriVerts;
		TriVerts.Add(F.a); TriVerts.Add(F.b); TriVerts.Add(F.c);
		TriVerts.Add(F.a); TriVerts.Add(F.c); TriVerts.Add(F.d);
		// merge check
		Afterfall::Math::CanMergeTriangles(TriVerts[0], TriVerts[1], TriVerts[2], TriVerts[3], TriVerts[4], TriVerts[5]);
		// store as one component regardless; tag with floor+face
		UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
		PMC->RegisterComponent();
		PMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		TArray<FVector> V = { F.a,F.b,F.c,F.d };
		TArray<int32> T = { 0,1,2, 0,2,3 };
		PMC->CreateMeshSection(0, V, T, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		PMC->ComponentTags.Add(FName(*F.tag));
		PMC->ComponentTags.Add(FName(*FString::Printf(TEXT("Floor%d"), FloorIndex)));
	}
}

void AProceduralBuilding::FragmentBuilding()
{
	TArray<UActorComponent*> Comps; GetComponents(Comps);
	for (UActorComponent* C : Comps)
	{
		if (UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C))
		{
			PMC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			PMC->SetSimulatePhysics(true);
			PMC->AddImpulse(FVector(FMath::FRandRange(-100.f,100.f), FMath::FRandRange(-100.f,100.f), FMath::FRandRange(0.f,200.f)), NAME_None, true);
		}
	}
	// TODO: replace with Chaos fracture / chunk splitting
}

void AProceduralBuilding::AddWallSegment(const TArray<FVector>& Triangles, const FString& Tag) {}