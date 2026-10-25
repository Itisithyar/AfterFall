#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AfterfallFixMapCommandlet.generated.h"

UCLASS()
class AFTERFALL_API UAfterfallFixMapCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UAfterfallFixMapCommandlet();
    virtual int32 Main(const FString& Params) override;
};
