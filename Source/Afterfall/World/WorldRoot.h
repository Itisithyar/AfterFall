// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: World - Root-connected persistent object system
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldRoot.generated.h"

UENUM(BlueprintType)
enum class EWorldObjectState : uint8
{
	Active,
	Stored,
	Carried,
	Deployed,
	Unloaded,
	Destroyed
};

USTRUCT(BlueprintType)
struct FWorldRootKV
{
	GENERATED_BODY()

	UPROPERTY() FString Key;
	UPROPERTY() FString Value;
};

USTRUCT(BlueprintType)
struct FWorldRootData
{
	GENERATED_BODY()
	
	UPROPERTY() FGuid RootId;
	UPROPERTY() FName ObjectType;
	UPROPERTY() FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY() FRotator WorldRotation = FRotator::ZeroRotator;
	UPROPERTY() EWorldObjectState State = EWorldObjectState::Active;
	UPROPERTY() TArray<FGuid> ChildRoots;
	UPROPERTY() TArray<FGuid> AttachedRoots;
	// NOTE (Kilo): TMap members are rejected by UHT inside a replicated struct,
	// so custom data is a replicated key/value array with identical semantics.
	UPROPERTY() TArray<FWorldRootKV> CustomData;
};

/** Persistent root object - independent of live instantiation */
UCLASS()
class AFTERFALL_API AWorldRoot : public AActor
{
	GENERATED_BODY()

public:
	AWorldRoot();

	UPROPERTY(Replicated, VisibleAnywhere, Category = "World")
	FGuid RootId;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "World")
	FWorldRootData RootData;

	UPROPERTY() TArray<TWeakObjectPtr<AActor>> LiveRepresentations;

	UFUNCTION(BlueprintCallable, Category = "World")
	void RegisterLiveRepresentation(AActor* LiveActor);
	
	UFUNCTION(BlueprintCallable, Category = "World")
	void UnregisterLiveRepresentation(AActor* LiveActor);
	
	UFUNCTION(BlueprintCallable, Category = "World")
	bool HasLiveRepresentation() const { return LiveRepresentations.Num() > 0; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
