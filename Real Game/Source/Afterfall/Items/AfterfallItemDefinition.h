#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AfterfallItemDefinition.generated.h"

UCLASS(BlueprintType)
class AFTERFALL_API UAfterfallItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Item") FName ItemID = NAME_None;
	UPROPERTY(EditAnywhere, Category="Item") FText DisplayName;
	UPROPERTY(EditAnywhere, Category="Item") FText Description;
	UPROPERTY(EditAnywhere, Category="Item") TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(EditAnywhere, Category="Item") TObjectPtr<UStaticMesh> WorldMesh = nullptr;
	UPROPERTY(EditAnywhere, Category="Item") float BaseWeight = 1.f;
	UPROPERTY(EditAnywhere, Category="Item") int32 MaxStackSize = 0;
	UPROPERTY(EditAnywhere, Category="Item") TArray<FName> Tags;
	UPROPERTY(EditAnywhere, Category="Item") TArray<FName> ParentIDs;
};
