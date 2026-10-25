#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/ItemStack.h"
#include "AfterfallSaveGame.generated.h"

UCLASS()
class AFTERFALL_API UAfterfallSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Placeholder = 0;
};