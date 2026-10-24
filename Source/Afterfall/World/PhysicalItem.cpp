// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "PhysicalItem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "AfterfallCharacter.h"
#include "Interaction/CarryComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Afterfall.h"

APhysicalItem::APhysicalItem()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetSimulatePhysics(true);
		Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}

	// Graybox silhouette per type (engine shapes only, no art dependency).
	const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
	if (ItemType == TEXT("AF_R_Wood") || ItemType == TEXT("res_wood_log"))
	{
		Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	}
	else if (ItemType == TEXT("AF_R_Stone"))
	{
		Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	}
	ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(Path);
	if (Asset.Succeeded())
	{
		Mesh->SetStaticMesh(Asset.Object);
		Mesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.35f));
	}
}

FVector APhysicalItem::ComputeReleaseVelocity(const FVector& PlayerVelocity, const FVector& Direction,
	float ThrowSpeed, float InheritFactor)
{
	const FVector Dir = Direction.GetSafeNormal();
	const float Speed = FMath::Max(ThrowSpeed, 0.f);
	return PlayerVelocity * InheritFactor + Dir * Speed;
}

FText APhysicalItem::GetInteractPrompt_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Take %s (E)"), *ItemType.ToString()));
}

bool APhysicalItem::CanInteract_Implementation(AController* /*Caller*/) const
{
	return WorldState == EItemWorldState::Free;
}

void APhysicalItem::ServerInteract_Implementation(AController* Caller)
{
	// E on a free item = pick it up through the holder's carry component.
	if (!HasAuthority() || !Caller)
	{
		return;
	}
	if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(Caller->GetPawn()))
	{
		if (C->Carry)
		{
			C->Carry->ServerPickup(this);
		}
	}
}

void APhysicalItem::ServerPickupBy(APawn* NewHolder)
{
	if (!HasAuthority() || !NewHolder || WorldState != EItemWorldState::Free)
	{
		return;
	}
	WorldState = EItemWorldState::Held;
	Holder = NewHolder;
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APhysicalItem::ServerReleaseToWorld(const FVector& ReleaseVelocity)
{
	if (!HasAuthority() || WorldState != EItemWorldState::Held)
	{
		return;
	}
	WorldState = EItemWorldState::Free;
	Holder = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(ReleaseVelocity);
	LastReleaseVelocity = ReleaseVelocity;
}

void APhysicalItem::ServerStore()
{
	// Stored = off-world; the mat slot owns the authoritative record now.
	if (!HasAuthority())
	{
		return;
	}
	WorldState = EItemWorldState::Stored;
	Holder = nullptr;
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void APhysicalItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APhysicalItem, ItemId);
	DOREPLIFETIME(APhysicalItem, ItemType);
	DOREPLIFETIME(APhysicalItem, WorldState);
	DOREPLIFETIME(APhysicalItem, Holder);
}
