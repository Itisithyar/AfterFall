// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "ResourceNode.h"
#include "Components/StaticMeshComponent.h"
#include "Inventory/InventoryComponent.h"
#include "AfterfallCharacter.h"
#include "Inventory/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/DateTime.h"
#include "Afterfall.h"

namespace
{
	const TCHAR* MeshPathFor(EResourceKind K)
	{
		// Engine basic shapes: zero project assets required for the slice.
		switch (K)
		{
		case EResourceKind::Stone: return TEXT("/Engine/BasicShapes/Sphere.Sphere");
		case EResourceKind::Hemp:  return TEXT("/Engine/BasicShapes/Cone.Cone");
		case EResourceKind::Wood:
		default:                   return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		}
	}

	FName ItemIdFor(EResourceKind K)
	{
		switch (K)
		{
		case EResourceKind::Stone: return TEXT("AF_R_Stone");
		case EResourceKind::Hemp:  return TEXT("AF_R_HempFiber");
		case EResourceKind::Wood:
		default:                   return TEXT("AF_R_Wood");
		}
	}
}

AResourceNode::AResourceNode()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Engine basic shape: loads on client and server alike (server needs it
	// for collision/trace authority). Kind stays fixed per spawned actor.
	ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(MeshPathFor(Kind));
	if (MeshAsset.Succeeded())
	{
		Mesh->SetStaticMesh(MeshAsset.Object);
	}
	switch (Kind)
	{
	case EResourceKind::Stone:
		Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.6f));
		break;
	case EResourceKind::Hemp:
		Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.2f));
		break;
	case EResourceKind::Wood:
	default:
		Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 2.5f));
		break;
	}
}

FText AResourceNode::GetInteractPrompt_Implementation() const
{
	if (IsDepleted())
	{
		return FText::FromString(TEXT("Depleted..."));
	}
	FString KindName;
	switch (Kind)
	{
	case EResourceKind::Stone: KindName = TEXT("Stone"); break;
	case EResourceKind::Hemp:  KindName = TEXT("Hemp"); break;
	case EResourceKind::Wood:
	default:                   KindName = TEXT("Wood"); break;
	}
	return FText::FromString(FString::Printf(TEXT("Harvest %s (E)"), *KindName));
}

bool AResourceNode::CanInteract_Implementation(AController* /*Instigator*/) const
{
	return !IsDepleted();
}

void AResourceNode::ServerInteract_Implementation(AController* Caller)
{
	if (!HasAuthority() || !Caller)
	{
		return;
	}
	APawn* Pawn = Caller->GetPawn();
	AAfterfallCharacter* Char = Cast<AAfterfallCharacter>(Pawn);
	if (!Pawn || !Char || !Char->Inventory)
	{
		return;
	}
	// Range re-check (3m + slack). Facing is checked by the caller's component.
	const float DistCm = FVector::Dist(Pawn->GetActorLocation(), GetActorLocation());
	if (DistCm > 450.f)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Harvest rejected: out of range %.0f"), DistCm);
		return;
	}
	if (IsDepleted())
	{
		return; // No duplicate grants: depleted latch set before grant below.
	}
	ServerHarvest(false);
	if (LastHarvestYield <= 0)
	{
		return;
	}
	// Latch depletion BEFORE granting so a re-entrant call cannot double-pay.
	DepletedUntilUnix = FDateTime::UtcNow().ToUnixTimestamp() + (int64)RespawnDelaySec;
	Char->Inventory->ServerAddItem(ItemIdFor(Kind), LastHarvestYield);
	// Legacy Wood mirror for the slice HUD.
	if (Kind == EResourceKind::Wood)
	{
		Char->Inventory->ServerAddResource(LastHarvestYield);
	}
	UE_LOG(LogAfterfall, Log, TEXT("Harvest granted %d %d (node %s)"),
		LastHarvestYield, (int32)Kind, *GetName());
}

int32 AResourceNode::YieldFor(EResourceKind K, float T01)
{
	const float T = FMath::Clamp(T01, 0.f, 0.9999f);
	switch (K)
	{
	case EResourceKind::Wood:  return 3 + FMath::FloorToInt(T * 3); // 3-5
	case EResourceKind::Stone: return 4 + FMath::FloorToInt(T * 5); // 4-8
	case EResourceKind::Hemp:  return 2 + FMath::FloorToInt(T * 3); // 2-4
	default: return 1;
	}
}

bool AResourceNode::IsDepleted() const
{
	if (DepletedUntilUnix <= 0)
	{
		return false;
	}
	return FDateTime::UtcNow().ToUnixTimestamp() < DepletedUntilUnix;
}

int32 AResourceNode::SliceYield(bool bHasTool) const
{
	int32 Yield = YieldMin + (YieldMax - YieldMin) / 2;
	if (bHasTool)
	{
		Yield = FMath::CeilToInt(Yield * 1.5f); // hatchet +50%
	}
	return FMath::Max(Yield, 0);
}

bool AResourceNode::ServerHarvest_Validate(bool /*bHasTool*/)
{
	return true;
}

void AResourceNode::ServerHarvest_Implementation(bool bHasTool)
{
	LastHarvestYield = 0;
	if (!HasAuthority() || IsDepleted())
	{
		return;
	}
	LastHarvestYield = SliceYield(bHasTool);
}

void AResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AResourceNode, DepletedUntilUnix);
	DOREPLIFETIME(AResourceNode, LastHarvestYield);
}
