// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: BEAM Free (Phase 1 medical) — data-driven medical item capabilities.
// Item IDs reuse the established loot/crafting/inventory identifiers
// (res_med_bandage, res_med_pills, res_med_kit, med_antiseptic,
// med_antibiotic, AF_MED_Bandage alias). No duplicate registry: this is the
// only capability table; loot tables own spawn, inventory owns counts.
#pragma once

#include "CoreMinimal.h"
#include "Combat/BodyDamageTypes.h"
#include "MedicalCapability.generated.h"

/** What one medical item can do when correctly applied to a wound. */
USTRUCT(BlueprintType)
struct FMedicalCapability
{
	GENERATED_BODY()

	/** Inventory item ID (matches loot/crafting/inventory IDs). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	FName ItemId = NAME_None;

	/** Stops active bleeding on the treated part. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	bool bStopsBleeding = false;

	/** HP restored on successful application (0 = dressing only). Applied
	 * through UHealthComponent::ServerHeal (recorded as PendingHPRestore
	 * by the treatment session until the server applies it — never
	 * written directly to HP by this table). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	float HPRestore = 0.f;

	/** Relieves pain flag on the treated part. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	bool bRelievesPain = false;

	/** Stabilizes severe injuries (enables natural recovery). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	bool bStabilizes = false;

	/** Hands-on application time in seconds (interruptible). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	float ApplicationSec = 3.f;

	/** Consumed exactly once, at successful completion (server-atomic). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Medical")
	bool bConsumeOnSuccess = true;
};

/** Static capability table + pure matchers (unit-tested, no world). */
UCLASS()
class AFTERFALL_API UMedicalCapabilityLib : public UObject
{
	GENERATED_BODY()

public:
	/** Full capability table (established item IDs only). */
	static const TArray<FMedicalCapability>& GetCapabilities();

	/** Capability for an item ID, or nullptr when unknown. */
	static const FMedicalCapability* FindCapability(FName ItemId);

	/** True when the item can help the given wound state. Pure. */
	static bool CapabilityMatchesInjury(const FMedicalCapability& Cap,
		bool bBleeding, bool bPain, int32 Severity, bool bStabilized);
};
