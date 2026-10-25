#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "Math/AfterfallMath.h"
class UPCGComponent;
class UPCGGraphInterface;
#include "ProceduralTerrain.generated.h"

UCLASS()
class AFTERFALL_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()
public:
	AProceduralTerrain();
	UPROPERTY(VisibleAnywhere, Category="Terrain") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category="Terrain") TObjectPtr<UProceduralMeshComponent> TerrainMesh;
    /** Optional UE PCG graph for Surface Sampler -> Static Mesh Spawner placement. */
    UPROPERTY(EditAnywhere, Category="Terrain|PCG") TObjectPtr<UPCGGraphInterface> PlacementGraph = nullptr;
    UPROPERTY(VisibleAnywhere, Category="Terrain|PCG") TObjectPtr<UPCGComponent> PlacementPCG = nullptr;
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TerrainConfig, Category="Terrain") int32 TileCountX = 10;
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TerrainConfig, Category="Terrain") int32 TileCountY = 10;
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TerrainConfig, Category="Terrain") float TileSize = 300.f;
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TerrainConfig, Category="Terrain") float HeightScale = 320.f;
	UPROPERTY(EditAnywhere, Category="Terrain") float RhombusShear = 0.0f;
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TerrainConfig, Category="Terrain") int32 RandomSeed = 12345;
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_TerrainConfig, Category="Terrain") bool bFlatMode = false;
	UPROPERTY(EditAnywhere, Category="Terrain") UMaterialInterface* TerrainMaterial = nullptr;
	UFUNCTION(BlueprintCallable, Category="Terrain") void RebuildTerrain();
protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_TerrainConfig();
};
