#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Math/AfterfallMath.h"
#include "ProceduralTerrain.generated.h"

UCLASS()
class AFTERFALL_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()
public:
	AProceduralTerrain();
	UPROPERTY(EditAnywhere, Category="Terrain") int32 TileCountX = 10;
	UPROPERTY(EditAnywhere, Category="Terrain") int32 TileCountY = 10;
	UPROPERTY(EditAnywhere, Category="Terrain") float TileSize = 300.f;
	UPROPERTY(EditAnywhere, Category="Terrain") float HeightScale = 320.f;
	UPROPERTY(EditAnywhere, Category="Terrain") float RhombusShear = 0.0f;
	UPROPERTY(EditAnywhere, Category="Terrain") int32 RandomSeed = 12345;
	UPROPERTY(EditAnywhere, Category="Terrain") bool bFlatMode = false;
	UPROPERTY(EditAnywhere, Category="Terrain") UMaterialInterface* TerrainMaterial = nullptr;
	UFUNCTION(BlueprintCallable, Category="Terrain") void RebuildTerrain();
protected:
	virtual void BeginPlay() override;
};