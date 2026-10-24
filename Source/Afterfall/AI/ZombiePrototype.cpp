// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "ZombiePrototype.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Combat/HealthComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"
#include "Persistence/PersistenceService.h"

AZombiePrototype::AZombiePrototype()
{
	bReplicates = true;
	HP = 100.f;
	// No per-frame tick: slow AI timer drives behavior (dormant = cheap).
	PrimaryActorTick.bCanEverTick = false;

	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	InitializeDefaultConfigs();

	// Humanoid placeholder (~180cm). Parts are trace-only; the capsule owns
	// movement collision so the model never blocks or separates.
	PartTorso = BuildPart(TEXT("Torso"), TEXT("/Engine/BasicShapes/Cube.Cube"),
		FVector(0.f, 0.f, 110.f), FVector(0.5f, 0.32f, 0.7f), TEXT("BodyPart.Torso"));
	PartHead = BuildPart(TEXT("Head"), TEXT("/Engine/BasicShapes/Sphere.Sphere"),
		FVector(0.f, 0.f, 168.f), FVector(0.3f, 0.3f, 0.32f), TEXT("BodyPart.Head"));
	PartArmL = BuildPart(TEXT("ArmL"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		FVector(0.f, -45.f, 115.f), FVector(0.12f, 0.12f, 0.7f), TEXT("BodyPart.ArmL"));
	PartArmR = BuildPart(TEXT("ArmR"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		FVector(0.f, 45.f, 115.f), FVector(0.12f, 0.12f, 0.7f), TEXT("BodyPart.ArmR"));
	PartLegL = BuildPart(TEXT("LegL"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		FVector(0.f, -16.f, 45.f), FVector(0.15f, 0.15f, 0.9f), TEXT("BodyPart.LegL"));
	PartLegR = BuildPart(TEXT("LegR"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		FVector(0.f, 16.f, 45.f), FVector(0.15f, 0.15f, 0.9f), TEXT("BodyPart.LegR"));
}

void AZombiePrototype::InitializeDefaultConfigs()
{
	DefaultWeaponConfig.WeaponId = FName("StandardSlice");
	DefaultWeaponConfig.AmmoType = FName("Standard");
	DefaultWeaponConfig.BaseDamage = 17.0f;
	DefaultWeaponConfig.bIsDesignatedSixShotWeapon = true;

	DefaultArmorConfig.ArmorType = FName("Unarmored");
	DefaultArmorConfig.DamageReduction = 0.0f;
}

UStaticMeshComponent* AZombiePrototype::BuildPart(const TCHAR* Name, const TCHAR* MeshPath,
	FVector Location, FVector Scale, const TCHAR* Tag)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(RootComponent);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	Part->ComponentTags.Add(Tag);
	ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(MeshPath);
	if (Asset.Succeeded())
	{
		Part->SetStaticMesh(Asset.Object);
	}
	return Part;
}

float AZombiePrototype::DamageForPart(EBodyPart Part)
{
	// Slice standard weapon: 6 torso hits kill (100/17), 3 headshots, ~8 limb hits.
	switch (Part)
	{
	case EBodyPart::Head:  return 34.f;
	case EBodyPart::Torso:  return 17.f;
	case EBodyPart::ArmL:
	case EBodyPart::ArmR:
	case EBodyPart::LegL:
	case EBodyPart::LegR:
	default:                 return 12.f;
	}
}

float AZombiePrototype::GetDamageForPart(EBodyPart Part) const
{
	if (DamageConfig)
	{
		return UDamageResolver::CalculatePartDamage(Part, DamageConfig->WeaponConfig, DamageConfig->BodyParts);
	}
	// Default six-shot baseline: torso is 17, 6 hits to kill 100 HP
	switch (Part)
	{
	case EBodyPart::Head:  return 34.f;
	case EBodyPart::Torso:  return 17.f;
	case EBodyPart::ArmL:
	case EBodyPart::ArmR:
	case EBodyPart::LegL:
	case EBodyPart::LegR:
	default:                 return 12.f;
	}
}

bool AZombiePrototype::PartFromComponent(const UPrimitiveComponent* Comp, EBodyPart& Out)
{
	if (!Comp)
	{
		return false;
	}
	static const TCHAR* Tags[6] = {
		TEXT("BodyPart.Head"), TEXT("BodyPart.Torso"), TEXT("BodyPart.ArmL"),
		TEXT("BodyPart.ArmR"), TEXT("BodyPart.LegL"), TEXT("BodyPart.LegR")
	};
	for (int32 i = 0; i < 6; ++i)
	{
		if (Comp->ComponentHasTag(Tags[i]))
		{
			Out = static_cast<EBodyPart>(i);
			return true;
		}
	}
	return false;
}

FGuid AZombiePrototype::GetOrCreateStableId()
{
	if (DamageRecord.bHasRecord && DamageRecord.StableId.IsValid())
	{
		StableId = DamageRecord.StableId;
		return StableId;
	}
	if (StableId.IsValid())
	{
		return StableId;
	}
	FGuid NewId = FGuid::NewGuid();
	StableId = NewId;
	return StableId;
}

void AZombiePrototype::EnsureDamageRecord()
{
	if (!DamageRecord.bHasRecord)
	{
		DamageRecord.bHasRecord = true;
		DamageRecord.StableId = GetOrCreateStableId();
		StableId = DamageRecord.StableId;
		// Initialize per-part health
		for (int32 i = 0; i < 6; ++i)
		{
			DamageRecord.PerPart[i] = 0.f;
			DamageRecord.bPartDamaged[i] = false;
			if (DamageConfig && DamageConfig->BodyParts.IsValidIndex(i))
			{
				DamageRecord.PerPartHealth[i] = DamageConfig->BodyParts[i].MaxHealth;
			}
			else
		{
			DamageRecord.PerPartHealth[i] = 100.f;
		}
	}		DamageRecord.TotalDealt = 0.f;
		DamageRecord.StatusEffects.Empty();
	}
}

FZombieGhost AZombiePrototype::ToGhost() const
{
	FZombieGhost G;
	G.StableId = DamageRecord.bHasRecord ? DamageRecord.StableId : (StableId.IsValid() ? StableId : FGuid());
	G.Home = GetActorLocation();
	G.HP = FMath::Max(FMath::RoundToInt(HP), 0);
	G.State = IsAlive() ? EEntitySimState::ActivePersistent : EEntitySimState::Resolved;
	return G;
}

void AZombiePrototype::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && GetWorld())
	{
		// 4Hz AI: cheap enough for a slice, dormant zombies gate via ShouldTickFull.
		GetWorld()->GetTimerManager().SetTimer(
			AITimer, this, &AZombiePrototype::AITick, 0.25f, true);
	}
}

void AZombiePrototype::EndPlay(EEndPlayReason::Type Reason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AITimer);
	}
	Super::EndPlay(Reason);
}

void AZombiePrototype::AITick()
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	APawn* Victim = nullptr;
	float VictimDistM = FLT_MAX;
	{
		APawn* Self = this;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AController* PC = It->Get())
			{
				if (APawn* P = PC->GetPawn())
				{
					if (P == Self)
					{
						continue;
					}
					if (UHealthComponent* H = P->FindComponentByClass<UHealthComponent>())
					{
						if (H->bIsDead)
						{
							continue;
						}
					}
					const float D = FVector::Dist(GetActorLocation(), P->GetActorLocation()) / 100.f;
					if (D < VictimDistM)
					{
						VictimDistM = D;
						Victim = P;
					}
				}
			}
		}
	}

	switch (State)
	{
	case EZombieState::Wander:
		GetCharacterMovement()->MaxWalkSpeed = WanderSpeed;
		if (Victim && VictimDistM <= SenseRangeM)
		{
			State = EZombieState::Chase;
			UE_LOG(LogAfterfall, Log, TEXT("Zombie %s pursues %s (%.0fm)"),
				*GetName(), *Victim->GetName(), VictimDistM);
		}
		else if (GetWorld()->TimeSeconds >= NextWanderTime)
		{
			NextWanderTime = GetWorld()->TimeSeconds + FMath::RandRange(4.f, 8.f);
			const FVector Fwd = GetActorForwardVector();
			const float Yaw = FMath::RandRange(-90.f, 90.f);
			WanderTarget = GetActorLocation() + Fwd.RotateAngleAxis(Yaw, FVector::UpVector) * 800.f;
		}
		if (!WanderTarget.IsZero())
		{
			const FVector Dir = (WanderTarget - GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(Dir, 1.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(),
				GetWorld()->GetDeltaSeconds(), 3.f));
		}
		break;

	case EZombieState::Chase:
		if (!Victim || VictimDistM > DisengageRangeM)
		{
			State = EZombieState::Wander; // lost: sensible disengage
			WanderTarget = FVector::ZeroVector;
			break;
		}
		if (VictimDistM <= AttackRangeM)
		{
			State = EZombieState::Attack;
			break;
		}
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		{
			// Unstick: if chasing but barely moving (wall/block), sidestep
			// perpendicular for a while instead of pushing forever.
			const float Now = GetWorld()->TimeSeconds;
		if (FVector::Dist(GetActorLocation(), StuckCheckPos) > 30.f
			|| Now - StuckCheckTime > 2.f)
		{
			StuckCheckPos = GetActorLocation();
			StuckCheckTime = Now;
			StuckSidestep = 0.f;
		}
		const FVector ToVictim = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (StuckSidestep <= 0.f && Now - StuckCheckTime >= 2.f
			&& FVector::Dist(GetActorLocation(), StuckCheckPos) <= 30.f)
		{
			StuckSidestep = 1.2f; // strafe right for ~1.2s, then re-check
			UE_LOG(LogAfterfall, Log, TEXT("Zombie %s sidesteps obstacle"), *GetName());
		}
		FVector Dir = ToVictim;
		if (StuckSidestep > 0.f)
		{
			StuckSidestep -= GetWorld()->GetDeltaSeconds();
			Dir = FVector(-ToVictim.Y, ToVictim.X, 0.f); // perpendicular
		}
		AddMovementInput(Dir, 1.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToVictim.Rotation(),
			GetWorld()->GetDeltaSeconds(), 5.f));
	}
		break;

	case EZombieState::Attack:
		if (!Victim || VictimDistM > AttackRangeM * 1.5f)
		{
			State = EZombieState::Chase; // stepped out: chase, not reset
			break;
		}
		{
			const FVector Dir = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			SetActorRotation(Dir.Rotation());
		}
		if (bCanAttack)
		{
			bCanAttack = false;
			if (UHealthComponent* H = Victim->FindComponentByClass<UHealthComponent>())
			{
				H->ServerTakeDamage(MeleeDamage);
				UE_LOG(LogAfterfall, Log, TEXT("Zombie %s hits %s for %.0f"),
					*GetName(), *Victim->GetName(), MeleeDamage);
			}
			FTimerHandle Hnd;
			GetWorld()->GetTimerManager().SetTimer(Hnd, [this]() { bCanAttack = true; }, 1.2f, false);
		}
		break;

	case EZombieState::Dead:
		break;
	}
}

bool AZombiePrototype::ServerHitPart_Validate(EBodyPart /*Part*/, AController* Attacker)
{
	return Attacker != nullptr;
}

void AZombiePrototype::ServerHitPart_Implementation(EBodyPart Part, AController* Attacker)
{
	if (!HasAuthority() || !IsAlive() || !Attacker)
	{
		return;
	}
	// Server range check: attacker pawn within 4m of the zombie.
	if (APawn* P = Attacker->GetPawn())
	{
		if (FVector::Dist(P->GetActorLocation(), GetActorLocation()) > 400.f)
		{
			UE_LOG(LogAfterfall, Warning, TEXT("HitPart rejected: out of range"));
			return;
		}
	}
	else
	{
		return;
	}
	ApplyDamage(DamageForPart(Part), static_cast<int32>(Part));
}

bool AZombiePrototype::ServerApplyDamage_Validate(EBodyPart /*Part*/, float Damage, AController* Attacker)
{
	return Attacker != nullptr && Damage >= 0.f && Damage <= 500.f;
}

void AZombiePrototype::ServerApplyDamage_Implementation(EBodyPart Part, float Damage, AController* Attacker)
{
	if (!HasAuthority() || !IsAlive() || !Attacker)
	{
		return;
	}
	// Same server range gate as the melee path: attacker within 4m.
	if (APawn* P = Attacker->GetPawn())
	{
		if (FVector::Dist(P->GetActorLocation(), GetActorLocation()) > 400.f)
		{
			UE_LOG(LogAfterfall, Warning, TEXT("ApplyDamage rejected: out of range"));
			return;
		}
	}
	else
	{
		return;
	}
	ApplyDamageWithPipeline(Part, Damage, Attacker);
}

void AZombiePrototype::ApplyDamageWithPipeline(EBodyPart Part, float BaseDamage, AController* /*Attacker*/)
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	EnsureDamageRecord();
	// Data-driven when a config asset is set, legacy 34/17/12 table otherwise
	// (resolver falls back to the table on empty BodyParts — same numbers).
	FWeaponDamageConfig Wep = DamageConfig ? DamageConfig->WeaponConfig : DefaultWeaponConfig;
	if (BaseDamage > 0.f)
	{
		Wep.BaseDamage = BaseDamage; // caller-supplied base wins
	}
	const TArray<FBodyPartConfig>& Parts = DamageConfig ? DamageConfig->BodyParts : DefaultBodyParts;
	const float Resolved = UDamageResolver::CalculatePartDamage(Part, Wep, Parts);
	const int32 Idx = UDamageResolver::GetPartIndex(Part);
	if (Idx >= 0 && Idx < 6)
	{
		DamageRecord.PerPart[Idx] += Resolved;
		DamageRecord.PerPartHealth[Idx] = FMath::Max(DamageRecord.PerPartHealth[Idx] - Resolved, 0.f);
		DamageRecord.bPartDamaged[Idx] = true;
	}
	// Aggregate HP stays authoritative (slice TotalHP rule); per-part state above
	// restores identically from RegionStateSave via ToGhost + record.
	ApplyDamage(Resolved, Idx);
}

bool AZombiePrototype::ServerTakeDamage_Validate(float Amount)
{
	return Amount >= 0.f && Amount <= 500.f;
}

void AZombiePrototype::ServerTakeDamage_Implementation(float Amount)
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	if (Amount > 0.f)
	{
	if (Amount > 0.f)
	{
		EnsureDamageRecord();
		ApplyDamage(Amount, static_cast<int32>(EBodyPart::Torso));
	}
}
}

void AZombiePrototype::ApplyDamage(float Amount, int32 PartIndex)
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	// First confirmed damage creates the one stable record; later hits update it.
	if (!DamageRecord.bHasRecord)
	{
		DamageRecord.bHasRecord = true;
		DamageRecord.StableId = FGuid::NewGuid();
		StableId = DamageRecord.StableId;
		for (int32 i = 0; i < 6; ++i)
		{
			DamageRecord.PerPart[i] = 0.f;
			DamageRecord.PerPartHealth[i] = 100.f;
			DamageRecord.bPartDamaged[i] = false;
		}
		DamageRecord.TotalDealt = 0.f;
	}
	if (PartIndex >= 0 && PartIndex < 6)
	{
		DamageRecord.PerPart[PartIndex] += Amount;
		DamageRecord.PerPartHealth[PartIndex] = FMath::Max(0.f, DamageRecord.PerPartHealth[PartIndex] - Amount);
		DamageRecord.bPartDamaged[PartIndex] = true;
	}
	DamageRecord.TotalDealt += Amount;
	HP = FMath::Max(HP - Amount, 0.f);
	if (HP <= 0.f)
	{
		Die(TEXT("PlayerMelee"));
	}
}

void AZombiePrototype::Die(const FString& Cause)
{
	if (State == EZombieState::Dead)
	{
		return;
	}
	State = EZombieState::Dead;
	GetWorld()->GetTimerManager().ClearTimer(AITimer);
	// Corpse: stop moving, drop collision, keep grey body visible 5s.
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);

	if (!bDeathRecorded)
	{
		bDeathRecorded = true;
	DeathRecord.DeathId = DamageRecord.bHasRecord ? DamageRecord.StableId : FGuid::NewGuid();
	DeathRecord.Position = GetActorLocation();
	DeathRecord.TimestampUnix = FDateTime::UtcNow().ToUnixTimestamp();
	DeathRecord.Cause = FName(*Cause);
	DeathRecord.ExpiresUnix = DeathRecord.TimestampUnix + 300;
		UE_LOG(LogAfterfall, Log, TEXT("Zombie died: id %s cause %s"),
			*DeathRecord.DeathId.ToString(), *Cause);
	}
	FTimerHandle CorpseTimer;
	GetWorld()->GetTimerManager().SetTimer(CorpseTimer, [this]()
	{
		if (!IsPendingKillPending())
		{
			Destroy();
		}
	}, 5.f, false);
}

bool AZombiePrototype::ShouldTickFull(float NearestPlayerDistM, bool bHasPursuer) const
{
	// 30m persistent zone always relevant; pursuer always relevant; else dormant.
	if (bHasPursuer)
	{
		return true;
	}
	return NearestPlayerDistM <= ActiveRangeM;
}

void AZombiePrototype::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZombiePrototype, HP);
	DOREPLIFETIME(AZombiePrototype, StableId);
}
