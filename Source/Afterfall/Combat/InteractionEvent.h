#pragma once
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "InteractionEvent.generated.h"

USTRUCT(BlueprintType)
struct FAfterfallInteractionEvent
{
	GENERATED_BODY()
	UPROPERTY() FGuid Id;
	UPROPERTY() double Timestamp = 0;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FVector Normal = FVector::UpVector;
	UPROPERTY() FVector Velocity = FVector::ZeroVector;
	UPROPERTY() FString Type;
	UPROPERTY() FString Material;
};