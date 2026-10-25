#pragma once
#include "CoreMinimal.h"
#include "ItemStack.generated.h"

USTRUCT(BlueprintType)
struct FItemStack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FString> DynamicProperties;
};
