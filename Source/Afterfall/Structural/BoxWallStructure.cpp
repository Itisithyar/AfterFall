// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: FLINT (GLM session) — see BoxWallStructure.h.
#include "BoxWallStructure.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Structural/BoxBuildingFallbackAssets.h"
#include "Combat/BodyRegionPhysics.h"

// Graybox cube asset: guaranteed engine content; the actor degrades
// gracefully (collision-only, no visuals) when unavailable.
namespace BoxWallRuntime
{
	static const TCHAR* CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** FIX-C (one-location fallback patch): try the intended fallback mesh
	 *  first, keep the guaranteed engine cube when null/unavailable so a
	 *  missing optional asset can never prevent launch. StaticLoadObject
	 *  returns the cached object when already loaded (no reload cost). */
	static UStaticMesh* CubeMesh(const FSoftObjectPath& Primary)
	{
		if (!Primary.IsNull())
		{
			if (UStaticMesh* M = Cast<UStaticMesh>(StaticLoadObject(
				UStaticMesh::StaticClass(), nullptr, *Primary.ToString())))
			{
				return M;
			}
		}
		// Packaged-safe default: /Game/ fallback paths are NOT cooked into
		// packaged builds unless hard-referenced; the engine cube always is.
		return Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, CubeMeshPath));
	}

	static void ApplyFallbackMaterial(UStaticMeshComponent* C, FName MaterialId)
	{
		if (!C) return;
		const FSoftObjectPath MatPath = BoxFallbackAssets::MaterialForId(MaterialId);
		if (MatPath.IsNull()) return; // e.g. glass: keep default, never null-crash
		if (UMaterialInterface* M = Cast<UMaterialInterface>(StaticLoadObject(
			UMaterialInterface::StaticClass(), nullptr, *MatPath.ToString())))
		{
			C->SetMaterial(0, M);
		}
	}
}

ABoxBlockDebris::ABoxBlockDebris()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false); // collision relay only; ledger replication is the wall's
}

void ABoxBlockDebris::ArmFrom(const FBoxBlock& Block, float MassKg,
	const FVector& ImpulseDir, float ImpactEnergyJ)
{
	// FIX (§3 mobility warning): debris mesh actors default to Static mobility;
	// SetStaticMesh/SimulatePhysics on a Static component warns every spawn.
	GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	if (UStaticMesh* Shard = BoxWallRuntime::CubeMesh(BoxFallbackAssets::DetachedBlockMesh()))
	{
		GetStaticMeshComponent()->SetStaticMesh(Shard);
		GetStaticMeshComponent()->SetWorldScale3D(FVector(0.55f)); // shrunken shard
		BoxWallRuntime::ApplyFallbackMaterial(GetStaticMeshComponent(),
			UBoxBuildingLib::MaterialId(Block.Material));
	}
	GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	GetStaticMeshComponent()->SetSimulatePhysics(true);
	GetStaticMeshComponent()->SetMassOverrideInKg(NAME_None, FMath::Max(1.f, MassKg), true);

	// Bounded momentum from the real impact energy; no arbitrary launch.
	const float VCmS = FMath::Min(
		FMath::Sqrt(2.f * FMath::Max(0.f, ImpactEnergyJ) / FMath::Max(1.f, MassKg)) * 100.f,
		MaxSpeedCmS);
	InheritedVelocity = ImpulseDir.GetSafeNormal() * VCmS;
	GetStaticMeshComponent()->SetPhysicsLinearVelocity(InheritedVelocity);
	LifeSeconds = 0.f;
}

void ABoxBlockDebris::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	LifeSeconds += DeltaTime;
	// Settle + kill budget so debris never accumulates unbounded.
	if (LifeSeconds > LifeMaxSeconds)
	{
		Destroy();
		return;
	}
	if (LifeSeconds > 3.f)
	{
		UPrimitiveComponent* PC = GetStaticMeshComponent();
		if (PC && PC->GetPhysicsLinearVelocity().IsNearlyZero(2.f))
		{
			Destroy();
		}
	}
}

ABoxWallStructure::ABoxWallStructure()
	: Super()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);
	SetReplicateMovement(false);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ABoxWallStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoxWallStructure, Profile);
	DOREPLIFETIME(ABoxWallStructure, Blocks);
	DOREPLIFETIME(ABoxWallStructure, DetachedBlockIds);
}

void ABoxWallStructure::ServerInitialize(const FBoxBuildingProfile& InProfile)
{
	if (!HasAuthority() || bInitialized)
	{
		return;
	}
	Profile = InProfile;
	UBoxBuildingLib::BuildLayout(Profile, Blocks);
	if (!UBoxBuildingLib::ValidateLayout(Profile, Blocks))
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxWall: invalid profile, wall denied"));
		Blocks.Reset();
		return;
	}
	DetachedBlockIds.Reset();
	bInitialized = true;
	RebuildComponents();
}

void ABoxWallStructure::BeginPlay()
{
	Super::BeginPlay();
	RebuildComponents(); // authority + replicated clients sync visuals
}

void ABoxWallStructure::OnRep_BlockState()
{
	// Client: mirror the server ledger (detached blocks lose visuals/collision).
	RebuildComponents();
}

void ABoxWallStructure::RebuildComponents()
{
	for (const FBoxBlock& B : Blocks)
	{
		const bool bDetached = DetachedBlockIds.Contains(B.Id);
		if (UStaticMeshComponent** Found = BlockComps.Find(B.Id))
		{
			(*Found)->SetVisibility(!bDetached);
			(*Found)->SetCollisionEnabled(bDetached
				? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
			continue;
		}
		auto* C = NewObject<UStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable); // runtime-spawned wall
		C->SetWorldScale3D(FVector(Profile.CubeSizeCm / 100.f));
		C->SetRelativeLocation(B.LocalCenter);
		C->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->OnComponentHit.AddDynamic(this, &ABoxWallStructure::OnBlockHit);
		C->AttachToComponent(GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform);
		C->RegisterComponent();
		if (UStaticMesh* Cube = BoxWallRuntime::CubeMesh(BoxFallbackAssets::IntactBlockMesh()))
		{
			C->SetStaticMesh(Cube);
			BoxWallRuntime::ApplyFallbackMaterial(C,
				UBoxBuildingLib::MaterialId(B.Material));
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("BoxWall: engine cube missing; collision-only block %d"), B.Id);
		}
		C->SetVisibility(!bDetached);
		if (bDetached) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BlockComps.Add(B.Id, C);
	}
}

int32 ABoxWallStructure::ServerApplyBoxImpact(const FVector& WorldImpactPoint,
	const FVector& WorldDirection, const FBoxImpactInput& Impact,
	const FGuid& EventId)
{
	if (!HasAuthority() || !bInitialized)
	{
		return 0;
	}
	// Event-level dedupe (bounded ledger), same discipline as StructuralActor.
	bool bSkipped = false;
	for (const FGuid& G : RecentEventIds)
	{
		if (G == EventId)
		{
			bSkipped = true;
			break;
		}
	}
	if (bSkipped)
	{
		UE_LOG(LogTemp, Log, TEXT("[IMPACT-DEDUP] guid=%s result=SKIPPED"), *EventId.ToString());
		return 0;
	}
	RecentEventIds.Add(EventId);
	while (RecentEventIds.Num() > 8)
	{
		RecentEventIds.RemoveAt(0);
	}
	UE_LOG(LogTemp, Log, TEXT("[IMPACT-DEDUP] guid=%s result=APPLIED"), *EventId.ToString());

	// World -> local (frame rule matches DestructionPropagation service docs).
	const FTransform Inv = GetActorTransform().Inverse();
	const FVector LocalPoint = Inv.TransformPosition(WorldImpactPoint);
	const FVector LocalDir = Inv.TransformVectorNoScale(WorldDirection);

	FBoxImpactInput In = Impact;
	In.ImpactLocalCenter = LocalPoint;
	In.Material = Profile.Material;
	const FBoxImpactResult R = UBoxBuildingLib::EvaluateImpact(Profile, Blocks, In);
	if (R.bDenied)
	{
		return 0;
	}
	Blocks = R.UpdatedBlocks;
	CommitResult(R, WorldDirection);
	DetachedBlockIds += R.RemovedBlocks;
	// FIX-A/B (ledger + replication): collapsed blocks are gone too — clients
	// hide visuals from DetachedBlockIds via OnRep_BlockState.
	for (int32 Id : R.CollapsedBlocks)
	{
		DetachedBlockIds.AddUnique(Id);
	}
	return R.RemovedBlocks.Num();
}

void ABoxWallStructure::CommitResult(const FBoxImpactResult& R, const FVector& DirWorld)
{
	for (const FBoxBlock& B : Blocks)
	{
		if (!R.RemovedBlocks.Contains(B.Id) && !R.CollapsedBlocks.Contains(B.Id))
		{
			continue;
		}
		// Hide the structural piece, detach the ledger state, and echo a
		// bounded physics shard so momentum continues physically.
		if (UStaticMeshComponent** C = BlockComps.Find(B.Id))
		{
			(*C)->SetVisibility(false);
			(*C)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (UWorld* W = GetWorld())
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FTransform At(GetActorTransform().TransformPosition(B.LocalCenter));
			const float MassKg = UBoxBuildingLib::BlockMassKg(Profile, B);
			const bool bCenter = (R.CenterBlockId == B.Id);
			const float EnergyJ = bCenter ? R.ImpactEnergyJ : R.ImpactEnergyJ * 0.35f;
			if (ABoxBlockDebris* D = W->SpawnActor<ABoxBlockDebris>(
				ABoxBlockDebris::StaticClass(), At, P))
			{
				D->ArmFrom(B, MassKg, DirWorld, EnergyJ);
			}
			// FIX-B: remote clients spawn their own visual echo.
			MulticastSpawnDebrisEcho(B.Id, MassKg, At.GetLocation(), DirWorld,
				EnergyJ, static_cast<int32>(B.Material));
		}
	}
}

void ABoxWallStructure::OnBlockHit(UPrimitiveComponent* /*HitComp*/,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector /*NormalImpulse*/, const FHitResult& Hit)
{
	// Authority-only contact route for thrown/simulating bodies; the
	// character strike path calls ServerApplyBoxImpact from ServerAttack.
	if (!HasAuthority() || !OtherActor || OtherActor == this)
	{
		return;
	}
	float MassKg = 75.f; // shared BodyRegion default body mass profile
	FVector VelOther = FVector::ZeroVector;
	if (UPrimitiveComponent* Prim = OtherComp)
	{
		VelOther = Prim->GetPhysicsLinearVelocity();
		// Simulating bodies supply real mass (lab blocks, debris shards).
		if (Prim->GetMass() > 1.f && Prim->IsSimulatingPhysics())
		{
			MassKg = Prim->GetMass();
		}
	}
	else if (OtherActor->GetRootComponent())
	{
		VelOther = OtherActor->GetVelocity();
	}
	const float SpeedCmS = VelOther.Size();
	if (SpeedCmS < 100.f)
	{
		return; // low-energy contacts (footsteps, brushes) are ignored
	}
	// Wall-level collision key: same body/wall/0.1s window share one key.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const int64 Key = (int64)(OtherActor ? OtherActor->GetUniqueID() : 0) * 7919LL
		+ (int64)GetUniqueID() * 104729LL
		+ (int64)(Now / 0.10);
	// expire old
	for (auto It = RecentImpactKeys.CreateIterator(); It; ++It)
	{
		if (It.Value() <= (float)Now) It.RemoveCurrent();
	}
	if (RecentImpactKeys.Contains(Key))
	{
		UE_LOG(LogTemp, Log, TEXT("[IMPACT-DEDUP] guid=key-%lld result=SKIPPED"), (long long)Key);
		return;
	}
	if (RecentImpactKeys.Num() >= 8)
	{
		// evict oldest
		int64 OldestKey = 0; float OldestTime = FLT_MAX;
		for (const auto& P : RecentImpactKeys)
		{
			if (P.Value < OldestTime) { OldestTime = P.Value; OldestKey = P.Key; }
		}
		RecentImpactKeys.Remove(OldestKey);
	}
	RecentImpactKeys.Add(Key, (float)(Now + 1.0));
	FBoxImpactInput In;
	In.EffectiveMassKg = MassKg * ThrownBodyEnergyScale;
	In.RelativeSpeedCmS = SpeedCmS;
	const FGuid EventId = FGuid::NewGuid();
	UE_LOG(LogTemp, Log, TEXT("[IMPACT] frame=%llu time=%f wall=%s other=%s guid=%s speed_cms=%f energy_J=%f"),
		(unsigned long long)GFrameCounter, (float)Now, *GetName(), OtherActor ? *OtherActor->GetName() : TEXT("None"), *EventId.ToString(), SpeedCmS, 0.5f * (MassKg * ThrownBodyEnergyScale) * FMath::Square(SpeedCmS / 100.f));
	ServerApplyBoxImpact(Hit.ImpactPoint, VelOther.GetSafeNormal(), In, EventId);
}

void ABoxWallStructure::MulticastSpawnDebrisEcho_Implementation(int32 BlockId, float MassKg,
	FVector SpawnWorldPos, FVector ImpulseDir, float ImpactEnergyJ, int32 MaterialIdx)
{
	if (HasAuthority() || !GetWorld())
	{
		return; // authority already spawned the physical shard
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABoxBlockDebris* D = GetWorld()->SpawnActor<ABoxBlockDebris>(
		ABoxBlockDebris::StaticClass(), FTransform(SpawnWorldPos), P))
	{
		FBoxBlock Echo;
		Echo.Id = BlockId;
		if (MaterialIdx >= 0 && MaterialIdx <= static_cast<int32>(EBoxBlockMaterial::Metal))
		{
			Echo.Material = static_cast<EBoxBlockMaterial>(MaterialIdx);
		}
		D->ArmFrom(Echo, MassKg, ImpulseDir, ImpactEnergyJ);
	}
}
