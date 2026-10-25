// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "PhysicalMat.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/CarryComponent.h"
#include "World/PhysicalItem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Afterfall.h"

APhysicalStorageMat::APhysicalStorageMat()
{
	bReplicates = true;
	// Repositioning on reopen (player wandered off) must reach clients.
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MatMesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Asset.Succeeded())
	{
		Mesh->SetStaticMesh(Asset.Object);
		Mesh->SetRelativeScale3D(FVector(2.f, 2.f, 0.2f));
	}
}

void APhysicalStorageMat::SetPacked(bool bInPacked)
{
	if (!HasAuthority())
	{
		return;
	}
	bPacked = bInPacked;
	OnRep_Packed();
}

void APhysicalStorageMat::OnRep_Packed()
{
	SetActorHiddenInGame(bPacked);
	SetActorEnableCollision(!bPacked);
}

int32 APhysicalStorageMat::ServerDeposit(AController* Caller)
{
	if (!HasAuthority() || !Caller || !bDeployed || bPacked)
	{
		return 0;
	}
	UCarryComponent* Carry = CarryOf(Caller);
	if (!Carry || !Carry->CarriedItem || !InReach(Caller))
	{
		return 0; // must be holding the item, near the mat
	}
	APhysicalItem* Item = Carry->CarriedItem;
	if (Item->GetHolder() != Caller->GetPawn())
	{
		return 0;
	}
	const FName Id = Item->ItemType;

	// --- validate first (no mutation yet) ---
	int32 SlotIdx = INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].ItemId == Id && Slots[i].Count < MaxStack)
		{
			SlotIdx = i;
			break;
		}
	}
	if (SlotIdx == INDEX_NONE)
	{
		if (Slots.Num() >= MaxSlots)
		{
			UE_LOG(LogAfterfall, Warning, TEXT("Mat deposit rejected: full"));
			return 0;
		}
	}

	// --- single commit point ---
	if (SlotIdx == INDEX_NONE)
	{
		FMatSlot S;
		S.ItemId = Id;
		S.Count = 1;
		S.InstanceId = Item->ItemId;
		Slots.Add(S);
	}
	else
	{
		Slots[SlotIdx].Count = FMath::Min(Slots[SlotIdx].Count + 1, MaxStack);
	}
	const FGuid KeptId = Item->ItemId;
	Item->ServerStore();
	Item->Destroy();
	// Clearing hands mirrors the authoritative move (item now lives in the slot).
	Carry->ServerClearAfterStore();
	UE_LOG(LogAfterfall, Log, TEXT("Mat deposit %s (%s)"), *Id.ToString(), *KeptId.ToString());
	return 1;
}

int32 APhysicalStorageMat::ServerWithdraw(AController* Caller, int32 SlotIndex, int32 Count)
{
	if (!HasAuthority() || !Caller || !bDeployed || bPacked)
	{
		return 0;
	}
	if (!Slots.IsValidIndex(SlotIndex) || Count <= 0 || !InReach(Caller))
	{
		return 0;
	}
	APawn* Pawn = Caller->GetPawn();
	if (!Pawn)
	{
		return 0;
	}
	FMatSlot& S = Slots[SlotIndex];
	if (S.Count <= 0)
	{
		return 0;
	}
	const int32 Take = FMath::Min(Count, S.Count);
	const FName Id = S.ItemId;
	const FGuid KeptId = S.InstanceId;

	// --- single commit point ---
	S.Count -= Take;
	if (S.Count <= 0)
	{
		Slots.RemoveAt(SlotIndex);
	}
	for (int32 i = 0; i < Take; ++i)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (APhysicalItem* Fresh = GetWorld()->SpawnActor<APhysicalItem>(
			APhysicalItem::StaticClass(),
			Pawn->GetActorLocation() + FVector(100.f, 0.f, 50.f),
			FRotator::ZeroRotator, P))
		{
			Fresh->ItemType = Id;
			Fresh->ItemId = (i == 0 && KeptId.IsValid()) ? KeptId : FGuid::NewGuid();
		}
	}
	UE_LOG(LogAfterfall, Log, TEXT("Mat withdraw %d x %s"), Take, *Id.ToString());
	return Take;
}

int32 APhysicalStorageMat::GetMatCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FMatSlot& S : Slots)
	{
		if (S.ItemId == ItemId)
		{
			Total += S.Count;
		}
	}
	return Total;
}

FContainerDelta APhysicalStorageMat::ExportDelta() const
{
	FContainerDelta D;
	D.ContainerGuid = ContainerGuid;
	D.LocationId = TEXT("MAT_DEPLOYED");
	D.TableVersion = 1;
	for (const FMatSlot& S : Slots)
	{
		D.Items.Add(S.ItemId);
		D.Quantities.Add(S.Count);
		D.Taken.Add(false);
	}
	return D;
}

UCarryComponent* APhysicalStorageMat::CarryOf(AController* Caller) const
{
	if (APawn* Pawn = Caller ? Caller->GetPawn() : nullptr)
	{
		return Pawn->FindComponentByClass<UCarryComponent>();
	}
	return nullptr;
}

bool APhysicalStorageMat::InReach(AController* Caller) const
{
	if (!Caller || !Caller->GetPawn())
	{
		return false;
	}
	return FVector::Dist(Caller->GetPawn()->GetActorLocation(), GetActorLocation()) <= 400.f;
}

void APhysicalStorageMat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APhysicalStorageMat, ContainerGuid);
	DOREPLIFETIME(APhysicalStorageMat, Slots);
	DOREPLIFETIME(APhysicalStorageMat, bDeployed);
	DOREPLIFETIME(APhysicalStorageMat, bPacked);
}
