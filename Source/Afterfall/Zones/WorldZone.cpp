// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "WorldZone.h"

AWorldZone::AWorldZone()
{
	bReplicates = true;
	ZoneId = FName("DefaultZone");
	ZoneName = "Default Zone";
}

void AWorldZone::LoadZone()
{
	bIsLoaded = true;
	// Load persistent objects in zone
}

void AWorldZone::UnloadZone()
{
	bIsLoaded = false;
	// Unload live representations but keep persistent roots
}
