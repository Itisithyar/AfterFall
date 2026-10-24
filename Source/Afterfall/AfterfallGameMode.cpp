// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallGameMode.h"
#include "Afterfall.h"

AAfterfallGameMode::AAfterfallGameMode()
{
	// Default pawn set in DefaultEngine.ini / project settings later.
	// Dedicated server is authoritative: bUseSeamlessTravel left default for now.
	UE_LOG(LogAfterfall, Log, TEXT("AfterfallGameMode created (Seed=%d)"), RegionSeed);
}
