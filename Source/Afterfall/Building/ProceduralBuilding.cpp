#include "ProceduralBuilding.h"
#include "Afterfall.h"
#include "Geometry/GeometryTypes.h"

AProceduralBuilding::AProceduralBuilding() { PrimaryActorTick.bCanEverTick = false; }

void AProceduralBuilding::BeginPlay() { Super::BeginPlay(); RebuildBuilding(); }

void AProceduralBuilding::RebuildBuilding()
{
	TArray<UActorComponent*> Comps; GetComponents(Comps);
	for (UActorComponent* C : Comps) if (UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C)) PMC->DestroyComponent();
	for (int32 f = 0; f < FloorCount; f++) BuildFloor(f);
}

void AProceduralBuilding::BuildFloor(int32 FloorIndex)
{
	float z = FloorIndex * FloorHeight;
	float hx = FootprintSize.X * 0.5f, hy = FootprintSize.Y * 0.5f;
	struct Face { FVector a,b,c,d; FString tag; };
	TArray<Face> Faces;
	Faces.Add({ FVector(-hx,-hy,z), FVector(hx,-hy,z), FVector(hx,-hy,z+FloorHeight), FVector(-hx,-hy,z+FloorHeight), TEXT("South") });
	Faces.Add({ FVector(hx,-hy,z), FVector(hx,hy,z), FVector(hx,hy,z+FloorHeight), FVector(hx,-hy,z+FloorHeight), TEXT("East") });
	Faces.Add({ FVector(hx,hy,z), FVector(-hx,hy,z), FVector(-hx,hy,z+FloorHeight), FVector(hx,hy,z+FloorHeight), TEXT("North") });
	Faces.Add({ FVector(-hx,hy,z), FVector(-hx,-hy,z), FVector(-hx,-hy,z+FloorHeight), FVector(-hx,hy,z+FloorHeight), TEXT("West") });
	Faces.Add({ FVector(-hx,-hy,z), FVector(hx,-hy,z), FVector(hx,hy,z), FVector(-hx,hy,z), TEXT("Floor") });
	for (const Face& F : Faces)
	{
		TArray<FRhombusQuad> Quads;
		FRhombusQuad Q;
		Q.V0=F.a; Q.V1=F.b; Q.V2=F.c; Q.V3=F.d;
		Q.UV0=FVector2D(0,0); Q.UV1=FVector2D(1,0); Q.UV2=FVector2D(1,1); Q.UV3=FVector2D(0,1);
		Q.Normal = FVector::CrossProduct(F.b-F.a, F.d-F.a).GetSafeNormal();
		Quads.Add(Q);
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> U; int32 Off=0;
		for (const FRhombusQuad& Qq : Quads) Qq.AppendToMeshBuffers(V,T,N,U,Off);
		UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
		PMC->RegisterComponent();
		PMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		PMC->CreateMeshSection(0, V, T, N, U, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
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