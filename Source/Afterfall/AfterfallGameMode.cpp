// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallGameMode.h"
#include "Afterfall.h"
#include "AfterfallCharacter.h"

AAfterfallGameMode::AAfterfallGameMode()
{
	// The player is the mechanics character. Movement, Enhanced Input, health,
	// inventory, carry, mat interaction, grapple and the menus all live on
	// AAfterfallCharacter; without this the map's template pawn is used and every
	// one of those systems is bypassed (the spawn path only ran from the slice
	// subsystem, respawn and the physics lab).
	DefaultPawnClass = AAfterfallCharacter::StaticClass();

	// Dedicated server is authoritative: bUseSeamlessTravel left default for now.
	UE_LOG(LogAfterfall, Log, TEXT("AfterfallGameMode created (Seed=%d) defaultPawn=%s"),
		RegionSeed, DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("null"));
}
