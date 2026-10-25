// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "MedicalCapability.h"

const TArray<FMedicalCapability>& UMedicalCapabilityLib::GetCapabilities()
{
	static const TArray<FMedicalCapability> Caps = []()
	{
		TArray<FMedicalCapability> C;
		// Dressing: stops bleeding, no HP. Craftable (2 cloth), loot MED.
		FMedicalCapability Bandage;
		Bandage.ItemId = TEXT("res_med_bandage");
		Bandage.bStopsBleeding = true;
		Bandage.ApplicationSec = 3.f;
		C.Add(Bandage);
		// Legacy alias carries identical capability (same physical item).
		FMedicalCapability BandageAlias = Bandage;
		BandageAlias.ItemId = TEXT("AF_MED_Bandage");
		C.Add(BandageAlias);
		// Field kit: stops bleeding + restores HP.
		FMedicalCapability Kit;
		Kit.ItemId = TEXT("res_med_kit");
		Kit.bStopsBleeding = true;
		Kit.HPRestore = 40.f;
		Kit.ApplicationSec = 5.f;
		C.Add(Kit);
		// Pills: pain relief only.
		FMedicalCapability Pills;
		Pills.ItemId = TEXT("res_med_pills");
		Pills.bRelievesPain = true;
		Pills.ApplicationSec = 1.5f;
		C.Add(Pills);
		// Antiseptic / antibiotic: stabilize severe wounds.
		FMedicalCapability Sep;
		Sep.ItemId = TEXT("med_antiseptic");
		Sep.bStabilizes = true;
		Sep.ApplicationSec = 2.5f;
		C.Add(Sep);
		FMedicalCapability Bio = Sep;
		Bio.ItemId = TEXT("med_antibiotic");
		C.Add(Bio);
		return C;
	}();
	return Caps;
}

const FMedicalCapability* UMedicalCapabilityLib::FindCapability(FName ItemId)
{
	for (const FMedicalCapability& C : GetCapabilities())
	{
		if (C.ItemId == ItemId)
		{
			return &C;
		}
	}
	return nullptr;
}

bool UMedicalCapabilityLib::CapabilityMatchesInjury(const FMedicalCapability& Cap,
	bool bBleeding, bool bPain, int32 Severity, bool bStabilized)
{
	// An action is offered only when the item changes something real.
	if (bBleeding && Cap.bStopsBleeding)
	{
		return true;
	}
	if (Cap.HPRestore > 0.f)
	{
		return true; // HP restoration is always a valid offer on a wound
	}
	if (bPain && Cap.bRelievesPain)
	{
		return true;
	}
	if (Severity >= 2 && !bStabilized && Cap.bStabilizes)
	{
		return true;
	}
	return false;
}
