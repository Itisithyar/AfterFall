// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallHUD.h"
#include "AfterfallCharacter.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interaction/InteractComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"

void AAfterfallHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}
	const float VS = Canvas->SizeY / 1080.f; // resolution-independent scale
	const float Pad = 24.f * VS;

	AAfterfallCharacter* Pawn = Cast<AAfterfallCharacter>(GetOwningPawn());
	if (!Pawn)
	{
		DrawText(TEXT("AFTERFALL - waiting for pawn..."), FLinearColor::Yellow,
			Pad, Pad, Font, 1.5f * VS);
		return;
	}

	// Crosshair dot.
	DrawText(TEXT("+"), FLinearColor::White,
		Canvas->SizeX * 0.5f - 6.f * VS, Canvas->SizeY * 0.5f - 14.f * VS,
		Font, 1.2f * VS);

	// Health bar (top-left).
	const float Ratio = Pawn->Health ? Pawn->Health->GetHPRatio() : 0.f;
	const float BW = 300.f * VS, BH = 26.f * VS;
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f), Pad, Pad, BW, BH);
	const FLinearColor HPCol = Ratio > 0.5f ? FLinearColor::Green
		: (Ratio > 0.25f ? FLinearColor::Yellow : FLinearColor::Red);
	DrawRect(HPCol, Pad, Pad, BW * Ratio, BH);
	DrawText(FString::Printf(TEXT("HP %d"),
		Pawn->Health ? FMath::RoundToInt(Pawn->Health->HP) : 0),
		FLinearColor::White, Pad + 8.f * VS, Pad + 2.f * VS, Font, 1.1f * VS);

	// Wood count under health.
	DrawText(FString::Printf(TEXT("Wood %d"),
		Pawn->Inventory ? Pawn->Inventory->GetWood() : 0),
		FLinearColor(1.f, 0.8f, 0.4f), Pad, Pad + BH + 8.f * VS, Font, 1.1f * VS);

	// Interaction prompt (bottom-center).
	if (Pawn->Interact && !Pawn->Interact->FocusedPrompt.IsEmpty())
	{
		const FString Prompt = Pawn->Interact->FocusedPrompt.ToString();
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Prompt, TW, TH, 1.3f * VS, 1.3f * VS);
		DrawText(Prompt, FLinearColor::White,
			Canvas->SizeX * 0.5f - TW * 0.5f, Canvas->SizeY - 140.f * VS,
			Font, 1.3f * VS);
	}

	// Death overlay.
	if (Pawn->Health && Pawn->Health->bIsDead)
	{
		const FString Msg = TEXT("YOU DIED — respawning...");
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Msg, TW, TH, 2.f * VS, 2.f * VS);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f),
			0.f, 0.f, (float)Canvas->SizeX, (float)Canvas->SizeY);
		DrawText(Msg, FLinearColor::Red,
			Canvas->SizeX * 0.5f - TW * 0.5f, Canvas->SizeY * 0.4f,
			Font, 2.f * VS);
	}
}
