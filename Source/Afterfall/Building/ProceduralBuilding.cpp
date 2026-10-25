#include "ProceduralBuilding.h"
#include "Afterfall.h"
#include "Components/SceneComponent.h"
#include "Geometry/GeometryTypes.h"

AProceduralBuilding::AProceduralBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	// Sections attach to this root: without one, AttachToComponent(nullptr)
	// silently no-ops and every section registers at the world origin instead
	// of the actor's spawn location (verified in afFragmentDemo PIE run).
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

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
		// Simple (convex) collision alongside the render mesh. A flat quad gives
		// Chaos a zero-volume hull it rejects, so SetSimulatePhysics() in
		// FragmentBuilding silently no-ops (afFragmentDemo showed simulating=0,
		// mass=1.0). An extruded box hull gives every section a simulatable body.
		{
			FBox Bounds(V);
			Bounds = Bounds.ExpandBy(FMath::Max(WallThickness, 1.f) * 0.5f);
			const FVector Mn = Bounds.Min, Mx = Bounds.Max;
			TArray<FVector> Corners;
			Corners.Reserve(8);
			Corners.Add(FVector(Mn.X, Mn.Y, Mn.Z)); Corners.Add(FVector(Mx.X, Mn.Y, Mn.Z));
			Corners.Add(FVector(Mx.X, Mx.Y, Mn.Z)); Corners.Add(FVector(Mn.X, Mx.Y, Mn.Z));
			Corners.Add(FVector(Mn.X, Mn.Y, Mx.Z)); Corners.Add(FVector(Mx.X, Mn.Y, Mx.Z));
			Corners.Add(FVector(Mx.X, Mx.Y, Mx.Z)); Corners.Add(FVector(Mn.X, Mx.Y, Mx.Z));
			PMC->AddCollisionConvexMesh(Corners);
		}
		PMC->ComponentTags.Add(FName(*F.tag));
		PMC->ComponentTags.Add(FName(*FString::Printf(TEXT("Floor%d"), FloorIndex)));
	}
}

FVector AProceduralBuilding::FragmentImpulseFor(const FVector& SectionCenter, const FVector& Epicenter,
	float Strength, float FalloffRadius, float JitterFraction, FRandomStream& Stream)
{
	const FVector Offset = SectionCenter - Epicenter;
	const float Dist = Offset.Size();
	// Degenerate (section sits on the epicenter): blast straight up so it clears
	// the rubble instead of receiving a zero impulse and staying put.
	const FVector Direction = (Dist > KINDA_SMALL_NUMBER) ? Offset / Dist : FVector::UpVector;
	const float Fall = (FalloffRadius > KINDA_SMALL_NUMBER)
		? FMath::Clamp(1.f - Dist / FalloffRadius, 0.f, 1.f)
		: 1.f;
	const FVector Jitter(Stream.FRandRange(-1.f, 1.f),
		Stream.FRandRange(-1.f, 1.f),
		Stream.FRandRange(-0.5f, 1.f));
	return Direction * (Strength * Fall) + Jitter * (Strength * FMath::Max(JitterFraction, 0.f));
}

void AProceduralBuilding::FragmentBuilding()
{
	if (bFragmented)
	{
		return; // already debris — do not re-kick settled pieces
	}
	bFragmented = true;

	// Seeded stream: the same building always shatters the same way, so a
	// replay of the blast is reproducible for tests and bug reports.
	FRandomStream Stream(RandomSeed);
	const FVector Epicenter = GetActorLocation();
	int32 NumSections = 0;
	TArray<UActorComponent*> Comps; GetComponents(Comps);
	for (UActorComponent* C : Comps)
	{
		if (UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C))
		{
			PMC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			PMC->SetSimulatePhysics(true);
			const FVector SecWorld = PMC->GetComponentLocation();
			const FVector Impulse = FragmentImpulseFor(SecWorld, Epicenter,
				FragmentStrength, FragmentFalloffRadius, FragmentJitter, Stream);
			// Velocity-change impulse: FragmentStrength is a speed (cm/s), so a
			// heavy slab and a thin panel scatter alike instead of Delta-v = J/m
			// ending up negligible on a multi-ton section.
			PMC->AddImpulse(Impulse, NAME_None, /*bVelChange=*/true);
			++NumSections;
		}
	}
	UE_LOG(LogAfterfall, Log, TEXT("FragmentBuilding: %d sections blasted from %s (strength %.0f, falloff %.0f)"),
		NumSections, *Epicenter.ToString(), FragmentStrength, FragmentFalloffRadius);
	// TODO: replace with Chaos fracture / chunk splitting
}

void AProceduralBuilding::AddWallSegment(const TArray<FVector>& Triangles, const FString& Tag) {}