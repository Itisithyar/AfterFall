#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Items/AfterfallItemDefinition.h"
#include "ItemRegistry.generated.h"

UCLASS()
class AFTERFALL_API UAfterfallItemRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UAfterfallItemDefinition* GetItem(FName ID) const;
	TArray<UAfterfallItemDefinition*> GetChildren(FName ID) const;
	UAfterfallItemDefinition* TryCombine(FName A, FName B) const;
	void RegisterItem(UAfterfallItemDefinition* Def);
	TMap<FName, UAfterfallItemDefinition*> NodesByID;
	TMap<FName, TArray<FName>> ChildrenByParent;
};
