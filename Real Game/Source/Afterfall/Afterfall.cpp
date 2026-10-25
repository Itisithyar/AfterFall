// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Afterfall.h"

DEFINE_LOG_CATEGORY(LogAfterfall);

void FAfterfallModule::StartupModule()
{
	UE_LOG(LogAfterfall, Log, TEXT("Afterfall module startup (UE 5.8, Windows client+server)"));
}

void FAfterfallModule::ShutdownModule()
{
	UE_LOG(LogAfterfall, Log, TEXT("Afterfall module shutdown"));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FAfterfallModule, Afterfall, "Afterfall");
