#include "AfterfallGameMode.h"
#include "Afterfall.h"
#include "AfterfallCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Building/ProceduralTerrain.h"
#include "Procedural/ProceduralObjectActor.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"

AAfterfallGameMode::AAfterfallGameMode()
{
	DefaultPawnClass = AAfterfallCharacter::StaticClass();
	UE_LOG(LogAfterfall, Log, TEXT("AfterfallGameMode created (Seed=%d) defaultPawn=%s"),
		RegionSeed, DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("null"));
}

AActor* AAfterfallGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	Super::ChoosePlayerStart_Implementation(Player);
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) return *It;
	FHitResult Hit;
	FVector Start = FVector(0,0,2000);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, FVector(0,0,-2000), ECC_Visibility);
	FVector Loc = Hit.bBlockingHit ? Hit.ImpactPoint + FVector(0,0,10) : FVector(0,0,0);
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* PS = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Loc, FRotator::ZeroRotator, P);
	UE_LOG(LogAfterfall, Log, TEXT("created a PlayerStart at %s"), *Loc.ToString());
	return PS;
}

void AAfterfallGameMode::BeginPlay()
{
	SuppressTemplateCubeGraybox();
	SpawnPlayableFloor();
	SpawnProceduralShowcase();
	GetWorldTimerManager().SetTimerForNextTick(this, &AAfterfallGameMode::ConfigureMorningLighting);
	if (bSpawnTestTerrain)
	{
		UWorld* W = GetWorld();
		if (W)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			W->SpawnActor<AProceduralTerrain>(AProceduralTerrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
	}
}

void AAfterfallGameMode::SuppressTemplateCubeGraybox()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client)
	{
		return;
	}
	int32 HiddenCubeActors = 0;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		const FString ActorName = It->GetName();
		const FString ClassName = It->GetClass() ? It->GetClass()->GetName() : FString();
		if (ActorName.Contains(TEXT("BP_DiamondSquare"), ESearchCase::IgnoreCase)
			|| ClassName.Contains(TEXT("BP_DiamondSquare"), ESearchCase::IgnoreCase))
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
			It->SetActorTickEnabled(false);
			UE_LOG(LogAfterfall, Log, TEXT("Disabled template cube generator %s; procedural AFTERFALL terrain owns the ground"),
				*ActorName);
		}
	}
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		if (It->GetName().Contains(TEXT("FLOOR_SpawnSafePlatform"), ESearchCase::IgnoreCase))
		{
			continue; // the floor placeholder is reused as hidden collision below the terrain
		}
		UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		UStaticMesh* Asset = Mesh ? Mesh->GetStaticMesh() : nullptr;
		if (Asset && Asset->GetName().Equals(TEXT("Cube"), ESearchCase::IgnoreCase))
		{
			Mesh->SetVisibility(false, true);
			Mesh->SetHiddenInGame(true, true);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			++HiddenCubeActors;
		}
	}
	if (HiddenCubeActors > 0)
	{
		UE_LOG(LogAfterfall, Log, TEXT("Removed %d template cube actors from the playable view"), HiddenCubeActors);
	}
}

void AAfterfallGameMode::SpawnPlayableFloor()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client) return;
	static const FName FloorTag(TEXT("AfterfallPlayableFloor2km"));
	AStaticMeshActor* ExistingFloor = nullptr;
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		if (It->ActorHasTag(FloorTag)) return;
	// The authored starter map already contains this named placeholder.
	// Reuse it as an invisible collision underlay instead of leaving its cube
	// mesh visible or adding a second floor actor.
		if (It->GetName().Contains(TEXT("FLOOR_SpawnSafePlatform"), ESearchCase::IgnoreCase))
		{
			ExistingFloor = *It;
		}
	}

	APlayerStart* Start = Cast<APlayerStart>(UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()));
	const FVector Anchor = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	// PlayerStart is the spawn reference. Use nearby real ground when it is
	// plausibly the spawn surface, placing the plane just above it; otherwise
	// put the plane at capsule-foot height rather than beneath a distant hit.
	// Keep a 2 km collision backup below the procedurally generated terrain.
	// The generated terrain mesh supplies the visible, walkable ground.
	const float ExpectedFootZ = Anchor.Z - 96.f;
	FHitResult Ground;
	FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(AFPlayableFloor), false);
	if (Start) GroundQuery.AddIgnoredActor(Start);
	if (ExistingFloor) GroundQuery.AddIgnoredActor(ExistingFloor);
	const bool bGroundHit = W->LineTraceSingleByChannel(Ground,
		Anchor + FVector(0.f, 0.f, 200.f), Anchor - FVector(0.f, 0.f, 10000.f),
		ECC_Visibility, GroundQuery);
	const bool bNearbyGround = bGroundHit && FMath::Abs(Ground.ImpactPoint.Z - ExpectedFootZ) <= 150.f;
	// The generated mesh has a 25 cm base above this hidden collision backup.
	const float FloorTop = bNearbyGround ? Ground.ImpactPoint.Z - 25.f : ExpectedFootZ - 25.f;
	const FVector FloorLocation(Anchor.X, Anchor.Y, FloorTop);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Floor = ExistingFloor;
	if (!Floor)
	{
		Floor = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FloorLocation,
			FRotator::ZeroRotator, Params);
	}
	if (Floor)
	{
		Floor->SetActorLocation(FloorLocation);
		Floor->SetActorRotation(FRotator::ZeroRotator);
		Floor->Tags.AddUnique(FloorTag);
		Floor->SetReplicates(true);
		Floor->SetReplicateMovement(false);
		if (UStaticMeshComponent* Mesh = Floor->GetStaticMeshComponent())
		{
			Mesh->SetMobility(EComponentMobility::Movable);
			if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
			{
				Mesh->SetStaticMesh(Plane);
				Mesh->SetWorldScale3D(FVector(2000.f, 2000.f, 1.f));
				Mesh->SetVisibility(false, true);
				Mesh->SetHiddenInGame(true, true);
				Mesh->SetCollisionProfileName(TEXT("BlockAll"));
				Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Mesh->SetGenerateOverlapEvents(false);
				Floor->SetActorHiddenInGame(false);
				Floor->SetActorEnableCollision(true);
				UBoxComponent* Collision = NewObject<UBoxComponent>(Floor, TEXT("AfterfallFloorCollision"));
				Collision->SetupAttachment(Mesh);
				Collision->SetBoxExtent(FVector(100000.f, 100000.f, 10.f));
				Collision->SetCollisionProfileName(TEXT("BlockAll"));
				Collision->SetIsReplicated(true);
				Collision->RegisterComponent();
				// The box is attached under a 2,000x mesh transform. Set its world
				// transform explicitly so the collider stays 2 km wide, not 2,000x
				// larger than the visible floor.
				Collision->SetWorldScale3D(FVector::OneVector);
				Collision->SetWorldLocation(FloorLocation - FVector(0.f, 0.f, 10.f));
				UE_LOG(LogAfterfall, Log, TEXT("Prepared hidden 2 km collision underlay at %s (topZ=%.1f, start=%s)"),
					*FloorLocation.ToString(), FloorTop, Start ? TEXT("PlayerStart") : TEXT("origin fallback"));
			}
			else
			{
				UE_LOG(LogAfterfall, Error, TEXT("Could not load UE BasicShapes Plane for playable floor"));
				Floor->Destroy();
			}
		}
	}
	else
	{
		UE_LOG(LogAfterfall, Error, TEXT("Failed to spawn the 2 km playable floor"));
	}
}

void AAfterfallGameMode::SpawnProceduralShowcase()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client)
	{
		return;
	}
	static const FName TerrainTag(TEXT("AfterfallPlayableTerrain2km"));
	static const FName ObjectTag(TEXT("AfterfallProceduralShowcase"));
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->ActorHasTag(TerrainTag) || It->ActorHasTag(ObjectTag))
		{
			return;
		}
	}
	APlayerStart* Start = Cast<APlayerStart>(UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()));
	const FVector Anchor = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	float FloorTop = Anchor.Z - 121.f;
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("AfterfallPlayableFloor2km"))))
		{
			FloorTop = It->GetActorLocation().Z;
			break;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform TerrainTransform(FRotator::ZeroRotator, FVector(Anchor.X, Anchor.Y, FloorTop));
	AProceduralTerrain* Terrain = W->SpawnActorDeferred<AProceduralTerrain>(
		AProceduralTerrain::StaticClass(), TerrainTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Terrain)
	{
		Terrain->Tags.Add(TerrainTag);
		Terrain->TileCountX = 200;
		Terrain->TileCountY = 200;
		Terrain->TileSize = 1000.f; // 10 m samples across a 2 km square
		Terrain->HeightScale = 60.f; // nearly flat walking surface, with seeded mathematical relief beyond the spawn apron
		Terrain->RandomSeed = RegionSeed;
		Terrain->bFlatMode = false;
		Terrain->FinishSpawning(TerrainTransform);
	}

	// Seeded SDF forms use the same deterministic mesh pipeline as deformable
	// and destructible objects. A small authored set makes the entry area feel
	// like a world, while keeping generation bounded and server authoritative.
	struct FShowcaseSpec
	{
		EShapeType32 Shape;
		EMaterialType Material;
		FVector Offset;
		float Radius;
		FVector HalfExtents;
		float Height;
		int32 SeedSalt;
	};
	const FShowcaseSpec Specs[] = {
		{ EShapeType32::Sphere, EMaterialType::Stone, FVector(500.f, 250.f, 10.f), 105.f, FVector(105.f), 100.f, 11 },
		{ EShapeType32::Capsule, EMaterialType::Concrete, FVector(650.f, -350.f, 15.f), 90.f, FVector(90.f), 110.f, 23 },
		{ EShapeType32::Cylinder, EMaterialType::Metal, FVector(-500.f, 300.f, 35.f), 75.f, FVector(75.f), 130.f, 37 }
	};
	const int32 SpecCount = static_cast<int32>(UE_ARRAY_COUNT(Specs));
	for (int32 Index = 0; Index < SpecCount; ++Index)
	{
		const FShowcaseSpec& Spec = Specs[Index];
		AProceduralObjectActor* Object = W->SpawnActor<AProceduralObjectActor>(
			AProceduralObjectActor::StaticClass(), Anchor + Spec.Offset, FRotator(0.f, Index * 47.f, 0.f), Params);
		if (!Object)
		{
			continue;
		}
		Object->Tags.Add(ObjectTag);
		FProceduralObjectDesc Desc;
		Desc.ObjectId = FGuid::NewGuid();
		Desc.RegionKey = TEXT("starter-region");
		Desc.Shape = Spec.Shape;
		Desc.GridSize = 32;
		Desc.CellSize = 10.f;
		Desc.Radius = Spec.Radius;
		Desc.HalfExtents = Spec.HalfExtents;
		Desc.Height = Spec.Height;
		Desc.NoiseStrength = 9.f;
		Desc.Seed = RegionSeed + Spec.SeedSalt;
		Desc.MaterialProfile = Spec.Material;
		Desc.CollisionMode = EProcObjCollision::StaticExact;
		Object->ServerAdoptDesc(Desc);
	}
	UE_LOG(LogAfterfall, Log, TEXT("Spawned seeded 2 km procedural terrain and %d SDF showcase objects"), SpecCount);
}

void AAfterfallGameMode::ConfigureMorningLighting()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client) return;
	ADirectionalLight* Sun = nullptr;
	for (TActorIterator<ADirectionalLight> It(W); It; ++It)
	{
		UDirectionalLightComponent* Light = It->GetComponentByClass<UDirectionalLightComponent>();
		if (It->IsHidden() || !Light) continue;
		if (!Sun)
		{
			Sun = *It;
		}
		else
		{
			// Multiple active directional lights overexpose this graybox and
			// trigger UE forward-shading conflicts. Keep one morning sun.
			Light->SetVisibility(false);
			UE_LOG(LogAfterfall, Warning, TEXT("Disabled duplicate directional light %s for single-sun morning setup"),
				*It->GetName());
		}
	}
	if (!Sun)
	{
		// Spawn at its default rotation. Passing the desired rotation here can
		// move the component before its Static mobility is changed below.
		Sun = W->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	if (Sun)
	{
		if (UDirectionalLightComponent* Light = Sun->GetComponentByClass<UDirectionalLightComponent>())
		{
			// Change the actual light component before moving its owning actor.
			// AActor mobility alone does not clear the component warning.
			Light->SetMobility(EComponentMobility::Movable);
			Sun->SetMobility(EComponentMobility::Movable);
			// Keep intensity close to the template's modest nonphysical range;
			// physical full-sun lux values overexpose this uncalibrated graybox.
			Light->SetIntensity(3.2f);
			Light->SetLightColor(FLinearColor(1.0f, 0.91f, 0.78f));
			Light->bUseTemperature = true;
			Light->Temperature = 5200.f;
		}
		Sun->SetReplicates(true);
		Sun->Tags.AddUnique(FName("AfterfallMorningSun"));
		Sun->SetActorRotation(FRotator(-22.f, 55.f, 0.f)); // morning sun directed down onto the level
	}
	ASkyLight* Sky = nullptr;
	for (TActorIterator<ASkyLight> It(W); It; ++It)
	{
		if (!It->IsHidden()) { Sky = *It; break; }
	}
	if (!Sky)
	{
		Sky = W->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	if (Sky)
	{
		Sky->SetReplicates(true);
		if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			SkyComponent->SetIntensity(0.7f);
			SkyComponent->RecaptureSky();
		}
	}
	else
	{
		UE_LOG(LogAfterfall, Error, TEXT("Failed to create morning directional sunlight"));
	}
	UE_LOG(LogAfterfall, Log, TEXT("Configured soft morning sunlight from above with ambient sky fill"));
}
