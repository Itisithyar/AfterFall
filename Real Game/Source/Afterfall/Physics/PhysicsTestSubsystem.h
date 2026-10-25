#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicsTestSubsystem.generated.h"

UCLASS()
class AFTERFALL_API UPhysicsTestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	void RunPhysicsTest();
};