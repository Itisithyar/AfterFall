// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: BEAM Free (mannequin foundation) — tunable per-body-region physics
// profiles keyed by the canonical EBodyPart model. Consumed today for
// weighted hit reactions (impulse scaled by region mass share); designed as
// the data contract for the future skeletal mannequin + grab interaction
// (socket names, mass distribution, constraint limits live here, not in
// combat code). Component tags follow the established BodyPart.* convention
// (see AZombiePrototype::PartFromComponent).
#pragma once

#include "CoreMinimal.h"
#include "Combat/BodyDamageTypes.h"
#include "BodyRegionPhysics.generated.h"

/** Tunable physical profile for one body region. */
USTRUCT(BlueprintType)
struct FBodyRegionPhysicsProfile
{
	GENERATED_BODY()

	/** Share of total body mass (sums to 1.0 across the six regions). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|BodyPhysics")
	float MassShare = 0.f;

	/** Socket/bone name on the future skeletal mesh (empty until art lands). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|BodyPhysics")
	FName SocketName = NAME_None;

	/** Grab stiffness multiplier (1.0 = nominal; hands/arms grip better). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|BodyPhysics")
	float GrabStiffness = 1.f;

	/** Impulse-to-velocity response scale (lighter regions move more). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|BodyPhysics")
	float ImpulseResponse = 1.f;

	/** Impact energy (J-equiv proxy) this region must carry to break fragile
	 * glass on contact. Heavier/faster hits break; light brushes do not. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|BodyPhysics")
	float GlassBreakEnergy = 100.f;
};

/** Static profile table + pure helpers (unit-tested, no world). */
UCLASS()
class AFTERFALL_API UBodyRegionPhysicsLib : public UObject
{
	GENERATED_BODY()

public:
	static const TArray<FBodyRegionPhysicsProfile>& GetProfiles();
	static const FBodyRegionPhysicsProfile* FindProfile(EBodyPart Part);

	/** Region mass in kg for a body of TotalMassKg. Pure. */
	static float RegionMassKg(EBodyPart Part, float TotalMassKg);

	/** Velocity change from an impulse applied to a region. Pure. */
	static float ImpulseVelocity(float ImpulseMag, EBodyPart Part, float TotalMassKg);

	/** True when the region, moving at SpeedCmS, can break fragile glass. Pure. */
	static bool CanBreakGlass(EBodyPart Part, float TotalMassKg, float SpeedCmS);
};
