// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "StructuralDestructionComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Destruction/DestructionPropagation.h"
#include "UObject/ConstructorHelpers.h"

UStructuralDestructionComponent::UStructuralDestructionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // visual only; authority is the actor's DetachedCells

	CellMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DamageCells"));
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		CellMesh->SetCollisionProfileName(TEXT("BlockAll"));
	}
	CellMesh->SetGenerateOverlapEvents(false);
	CellMesh->SetCanEverAffectNavigation(false);
	ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Asset.Succeeded())
	{
		CellMesh->SetStaticMesh(Asset.Object);
	}
}

void UStructuralDestructionComponent::OnRegister()
{
	Super::OnRegister();
	if (CellMesh && GetOwner() && GetOwner()->GetRootComponent())
	{
		if (!CellMesh->IsRegistered())
		{
			CellMesh->RegisterComponent();
		}
		if (CellMesh->GetAttachParent() == nullptr)
		{
			CellMesh->AttachToComponent(GetOwner()->GetRootComponent(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			CellMesh->SetRelativeTransform(FTransform::Identity);
		}
	}
}

void UStructuralDestructionComponent::Rebuild(const TArray<int32>& DetachedCells)
{
	if (!CellMesh)
	{
		return;
	}
	// Contiguous graybox cells: lattice spacing Extent, cube edge 2*Extent/3.
	// Cube.Cube is 100cm, so scale = edge/100.
	const float Edge = CellExtentCm * 2.f / 3.f;
	const float Scale = Edge / 100.f;
	CellMesh->ClearInstances();
	BuiltDetached.Reset();
	for (int32 C : DetachedCells)
	{
		if (C >= 0 && C < UDestructionPropagationService::GridPointCount())
		{
			BuiltDetached.Add(C);
		}
	}
	for (int32 IZ = 0; IZ < 3; ++IZ)
	{
		for (int32 IY = 0; IY < 3; ++IY)
		{
			for (int32 IX = 0; IX < 3; ++IX)
			{
				const int32 Idx = UDestructionPropagationService::GridIndex(IX, IY, IZ);
				if (BuiltDetached.Contains(Idx))
				{
					continue; // detached: hole stays open, no instance, no collision
				}
				const FVector Local = UDestructionPropagationService::GridPoint(
					FVector::ZeroVector, CellExtentCm, IX, IY, IZ);
				CellMesh->AddInstance(FTransform(FRotator::ZeroRotator, Local,
					FVector(Scale, Scale, Scale)));
			}
		}
	}
}

int32 UStructuralDestructionComponent::VisibleCellCount() const
{
	return CellMesh ? CellMesh->GetInstanceCount() : 0;
}

bool UStructuralDestructionComponent::IsInSync(const TArray<int32>& DetachedCells) const
{
	TArray<int32> Valid;
	for (int32 C : DetachedCells)
	{
		if (C >= 0 && C < UDestructionPropagationService::GridPointCount() && !Valid.Contains(C))
		{
			Valid.Add(C);
		}
	}
	return SameSet(BuiltDetached, Valid);
}

bool UStructuralDestructionComponent::SameSet(const TArray<int32>& A, const TArray<int32>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	for (int32 C : A)
	{
		if (!B.Contains(C))
		{
			return false;
		}
	}
	return true;
}
