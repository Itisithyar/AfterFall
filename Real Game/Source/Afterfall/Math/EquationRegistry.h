#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EquationRegistry.generated.h"

USTRUCT(BlueprintType)
struct FAfterfallEquation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float A = 0, B = 0, C = 0, D = 0;
	float Eval(float T) const { return A*T*T + B*T + C; }
	float EvalClamped(float T, float Min, float Max) const { return FMath::Clamp(Eval(T), Min, Max); }
};

UCLASS()
class AFTERFALL_API UAfterfallEquationRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	FAfterfallEquation GetEquation(FName ID) const;
	float EvalEquation(FName ID, float T) const;
	TMap<FName, FAfterfallEquation> Equations;
};
