#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AfterfallSettingsComponent.generated.h"

UCLASS()
class AFTERFALL_API UAfterfallSettingsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAfterfallSettingsComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings") float FOV = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings") bool bAdminDebug = false;
	UFUNCTION(BlueprintCallable) void ApplyFOV();
	UFUNCTION(BlueprintCallable) void ToggleAdminDebug();
};