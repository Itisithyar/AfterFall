#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Math/AfterfallMath.h"
#include "ProceduralBuilding.generated.h"

UCLASS()
class AFTERFALL_API AProceduralBuilding : public AActor
{
	GENERATED_BODY()
public:
	AProceduralBuilding();
	UPROPERTY(EditAnywhere, Category="Building") int32 FloorCount = 3;
	UPROPERTY(EditAnywhere, Category="Building") FVector2D FootprintSize = FVector2D(900,900);
	UPROPERTY(EditAnywhere, Category="Building") float WallThickness = 20.f;
	UPROPERTY(EditAnywhere, Category="Building") float FloorHeight = 300.f;
	UPROPERTY(EditAnywhere, Category="Building") int32 RandomSeed = 12345;
	UFUNCTION(BlueprintCallable, Category="Building") void RebuildBuilding();
	UFUNCTION(BlueprintCallable, Category="Building") void FragmentBuilding();
protected:
	virtual void BeginPlay() override;
private:
	void BuildFloor(int32 FloorIndex);
	void AddWallSegment(const TArray<FVector>& Triangles, const FString& Tag);
};