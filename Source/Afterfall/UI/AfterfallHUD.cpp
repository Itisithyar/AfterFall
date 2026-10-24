// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Quick menu (Part A): compact top-left Canvas panel. Zero UMG assets so it
// works in every build. Animation uses real time so it plays while paused.
#include "AfterfallHUD.h"
#include "AfterfallCharacter.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interaction/InteractComponent.h"
#include "AI/ZombiePrototype.h"
#include "Physics/PhysicsValidationSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

AAfterfallHUD::AAfterfallHUD()
{
	// Capture-mode key polling + close-fade need ticks even while paused.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}void AAfterfallHUD::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AAfterfallCharacter* P = MenuPawn();
	if (!P || !P->IsQuickMenuOpen())
	{
		return;
	}
	// While paused, actor input bindings are halted — the HUD (ticking even
	// when paused) owns Escape-close, LMB click routing, and gamepad
	// confirm here so the menu stays usable. Character handlers for the
	// same buttons are fallback for any unpaused-menu configuration.
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape))
		{
			if (P->ControlsCaptureAction != NAME_None)
			{
				P->ControlsCaptureAction = NAME_None;
			}
			else
			{
				P->CloseQuickMenu();
				return;
			}
		}
		if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			HandleMenuClick();
		}
		else if (PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
		{
			ActivateSelectedButton();
		}
	}
	if (P->ControlsCaptureAction != NAME_None)
	{
		PollCaptureKey(P);
	}
}

AAfterfallCharacter* AAfterfallHUD::MenuPawn() const
{
	return Cast<AAfterfallCharacter>(GetOwningPawn());
}

void AAfterfallHUD::PollCaptureKey(AAfterfallCharacter* Pawn)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->PlayerInput)
	{
		return;
	}
	// Escape cancels capture (and must not be rebound by the capture).
	if (PC->WasInputKeyJustPressed(EKeys::Escape))
	{
		Pawn->ControlsCaptureAction = NAME_None;
		return;
	}
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	for (const FKey& K : AllKeys)
	{
		if (!K.IsValid() || K.IsModifierKey() || K.IsGamepadKey() || K.IsMouseButton())
		{
			continue;
		}
		if (PC->WasInputKeyJustPressed(K))
		{
			Pawn->RemapAction(Pawn->ControlsCaptureAction, K);
			return;
		}
	}
}

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
	}	// Weapon + zombie count under wood.
	DrawText(FString::Printf(TEXT("Ammo %d/%d"), Pawn->AmmoInMag, Pawn->MagSize),
		FLinearColor(1.f, 1.f, 0.6f), Pad, Pad + (BH + 8.f * VS) * 2, Font, 1.1f * VS);
	int32 AliveZ = 0;
	for (TActorIterator<AZombiePrototype> Zit(GetWorld()); Zit; ++Zit)
	{
		if (Zit->IsAlive())
		{
			++AliveZ;
		}
	}
	DrawText(FString::Printf(TEXT("Zombies %d"), AliveZ),
		FLinearColor(1.f, 0.5f, 0.5f), Pad, Pad + (BH + 8.f * VS) * 3, Font, 1.1f * VS);

	// Death overlay (menu offers the immediate Respawn button).
	if (Pawn->Health && Pawn->Health->bIsDead)
	{
		const FString Msg = TEXT("YOU DIED — Esc for menu / auto-respawn...");
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Msg, TW, TH, 2.f * VS, 2.f * VS);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f),
			0.f, 0.f, (float)Canvas->SizeX, (float)Canvas->SizeY);
		DrawText(Msg, FLinearColor::Red,
				Canvas->SizeX * 0.5f - TW * 0.5f, Canvas->SizeY * 0.4f,
				Font, 2.f * VS);
	}

	// Quick menu (top-left, content-sized, game visible around it).
	DrawQuickMenu(Pawn, Font, VS, FPlatformTime::Seconds());
}

void AAfterfallHUD::DrawQuickMenu(AAfterfallCharacter* Pawn, UFont* Font, float VS, double Now)
{
	const bool bOpen = Pawn->IsQuickMenuOpen();
	if (bOpen)
	{
		bWasMenuOpen = true;
	}
	else if (bWasMenuOpen)
	{
		// Close fade: panel lingers ~0.12s, non-interactive, game already live.
		bWasMenuOpen = false;
		MenuCloseAnimEnd = Now + 0.12;
	}
	float Alpha = 1.f;
	float SlidePx = 0.f;
	if (bOpen)
	{
		const double T = (Now - Pawn->MenuAnimStart) / 0.15; // 150ms open
		Alpha = (float)FMath::Clamp(T, 0.0, 1.0);
		SlidePx = (1.f - Alpha) * 12.f * VS;
	}
	else
	{
		const double T = (MenuCloseAnimEnd - Now) / 0.12;
		if (T <= 0.0)
		{
		MenuButtons.Reset();
			return;
		}
		Alpha = (float)FMath::Clamp(T, 0.0, 1.0);
	}
	MenuButtons.Reset();
	const float MW = FMath::Min(280.f * VS, (float)Canvas->SizeX * 0.7f);
	const float MX = 16.f * VS - SlidePx, MY = 16.f * VS;
	const float RowH = 34.f * VS, HeadH = 30.f * VS;
	const float PadX = 12.f * VS;

	// Page content determines height: build button list first.
	TArray<FMenuButton> Buttons;
	const bool bDead = Pawn->HealthIsDead();
	const EQuickMenuPage Page = Pawn->QuickMenuPage;
	if (Page == EQuickMenuPage::Main)
	{
		Buttons.Add(FMenuButton{ FName(TEXT("Resume")), TEXT("Resume") });
		Buttons.Add(FMenuButton{ FName(TEXT("Map")), TEXT("Map") });
		Buttons.Add(FMenuButton{ FName(TEXT("Controls")), TEXT("Change Controls") });
		Buttons.Add(FMenuButton{ FName(TEXT("Lab")), TEXT("Physics Lab") });
		FMenuButton R{ FName(TEXT("Respawn")), TEXT("Respawn") };
		R.bEnabled = bDead;
		Buttons.Add(R);
		Buttons.Add(FMenuButton{ FName(TEXT("Close")), TEXT("Close") });
	}
	else if (Page == EQuickMenuPage::Controls)
	{
		TArray<FActionBindingRow> Rows;
		Pawn->GetBindingRows(Rows);
		for (const FActionBindingRow& R : Rows)
		{
		FMenuButton B;
		if (R.bRemappable)
		{
			B.Id = FName(*(TEXT("Capture:") + R.ActionName.ToString()));
			B.Label = R.ActionName.ToString() + TEXT(": ") +
				(Pawn->ControlsCaptureAction == R.ActionName
					? TEXT("press a key...") : R.KeyName);
		}
		else
		{
			B.Id = FName(TEXT("Noop"));
			B.Label = R.ActionName.ToString() + TEXT(": ") + R.KeyName;
			B.bEnabled = false;
		}
		Buttons.Add(B);
	}
		Buttons.Add(FMenuButton{ FName(TEXT("RestoreDefaults")), TEXT("Restore defaults") });
		Buttons.Add(FMenuButton{ FName(TEXT("Back")), TEXT("Back") });
	}
	else
	{
		// Map + Lab pages render custom bodies below; buttons appended after.		if (Page == EQuickMenuPage::Lab)
		{
			Buttons.Add(FMenuButton{ FName(TEXT("LabRun")), TEXT("Run 10kg drop test") });
			Buttons.Add(FMenuButton{ FName(TEXT("LabLog")), TEXT("Toggle impact logging") });
		}
		Buttons.Add(FMenuButton{ FName(TEXT("Back")), TEXT("Back") });
	}
	// Clamp gamepad selection into range.
	Pawn->MenuSelectedIndex = FMath::Clamp(Pawn->MenuSelectedIndex, 0, Buttons.Num() - 1);

	float ExtraH = 0.f;
	if (Page == EQuickMenuPage::Map)
	{
		ExtraH = 220.f * VS;
	}
	else if (Page == EQuickMenuPage::Lab)
	{
		ExtraH = 96.f * VS;
	}
	else if (Page == EQuickMenuPage::Controls)
	{
		ExtraH = 0.f; // rows are buttons
	}
	const float MH = HeadH + ExtraH + Buttons.Num() * (RowH + 6.f * VS) + 12.f * VS;

	// Panel: dark translucent, subtle border, content-sized.
	DrawRect(FLinearColor(0.05f, 0.06f, 0.08f, 0.88f * Alpha), MX, MY, MW, MH);
	DrawRect(FLinearColor(0.4f, 0.45f, 0.55f, 0.9f * Alpha), MX, MY, MW, 1.f);
	DrawRect(FLinearColor(0.4f, 0.45f, 0.55f, 0.9f * Alpha), MX, MY + MH - 1.f, MW, 1.f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha),
		MX + PadX, MY + 8.f * VS, MW - PadX * 2.f, 20.f * VS);
	DrawText(TEXT("AFTERFALL"), FLinearColor(0.05f, 0.06f, 0.08f),
		MX + PadX + 4.f * VS, MY + 8.f * VS, Font, 1.0f * VS);

	float Y = MY + HeadH;
	if (Page == EQuickMenuPage::Map)
	{
		DrawMapPage(FBox2D(FVector2D(MX + PadX, Y), FVector2D(MX + PadX + MW - PadX * 2.f, Y + ExtraH)), Font, VS);
		Y += ExtraH;
	}
	else if (Page == EQuickMenuPage::Lab)
	{
		DrawLabPage(FBox2D(FVector2D(MX + PadX, Y), FVector2D(MX + PadX + MW - PadX * 2.f, Y + ExtraH)), Font, VS);
		Y += ExtraH;
	}
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		FMenuButton B = Buttons[i];
		B.Rect = FBox2D(FVector2D(MX + PadX, Y), FVector2D(MX + PadX + MW - PadX * 2.f, Y + RowH));
		MenuButtons.Add(B);
		DrawMenuButton(i, B, bOpen ? Pawn->MenuSelectedIndex : -1, Font, VS, Alpha);
		Y += RowH + 6.f * VS;
	}
}

void AAfterfallHUD::DrawMenuButton(int32 Index, const FMenuButton& B, int32 Selected, UFont* Font, float VS, float Alpha)
{
	(void)Index;
	const bool bHi = (Index == Selected);
	const FLinearColor Bg = !B.bEnabled ? FLinearColor(0.15f, 0.15f, 0.15f, 0.6f * Alpha)
		: (bHi ? FLinearColor(0.25f, 0.45f, 0.75f, 0.95f * Alpha)
		         : FLinearColor(0.16f, 0.18f, 0.22f, 0.9f * Alpha));
	DrawRect(Bg, B.Rect.Min.X, B.Rect.Min.Y, B.Rect.GetSize().X, B.Rect.GetSize().Y);
	DrawText(B.Label, B.bEnabled ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f),
		B.Rect.Min.X + 8.f * VS, B.Rect.Min.Y + 6.f * VS, Font, 1.0f * VS);
}

void AAfterfallHUD::DrawMapPage(const FBox2D& Inner, UFont* Font, float VS)
{
	// Graybox overview: fixed ±2600cm world window, player + zombie dots.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.04f, 1.f), Inner.Min.X, Inner.Min.Y, Inner.GetSize().X, Inner.GetSize().Y);
	const FVector2D C(Inner.Min.X + Inner.GetSize().X * 0.5f, Inner.Min.Y + Inner.GetSize().Y * 0.5f);
	const float Scale = (Inner.GetSize().X * 0.5f) / 2600.f;
	auto Dot = [&](const FVector& Loc, const FLinearColor& Col, float R)
	{
		const float X = C.X + Loc.X * Scale;
		const float Y = C.Y - Loc.Y * Scale; // map-north up
		DrawRect(Col, X - R, Y - R, R * 2.f, R * 2.f);
	};
	if (APawn* P = GetOwningPawn())
	{
		Dot(P->GetActorLocation(), FLinearColor::Green, 3.f * VS);
	}
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AZombiePrototype> Zit(W); Zit; ++Zit)
		{
			Dot(Zit->GetActorLocation(), Zit->IsAlive() ? FLinearColor::Red : FLinearColor(0.4f, 0.4f, 0.4f), 2.5f * VS);
		}
	}
	DrawText(TEXT("Map: green=you red=zombies (Esc=back)"), FLinearColor(0.8f, 0.8f, 0.8f),
		Inner.Min.X + 4.f * VS, Inner.Min.Y + Inner.GetSize().Y - 20.f * VS, Font, 0.8f * VS);
}

void AAfterfallHUD::DrawLabPage(const FBox2D& Inner, UFont* Font, float VS)
{
	FString Summary = TEXT("Lab idle — run the 10kg drop test.");
	bool bLog = false;
	if (UWorld* W = GetWorld())
	{
		if (UPhysicsValidationSubsystem* S = W->GetSubsystem<UPhysicsValidationSubsystem>())
		{
		Summary = S->GetLabSummary();
		bLog = S->IsLabLogging();
		}
	}
	DrawText(Summary, FLinearColor(0.9f, 0.9f, 0.7f),
		Inner.Min.X + 4.f * VS, Inner.Min.Y + 4.f * VS, Font, 0.8f * VS);
	DrawText(bLog ? TEXT("Impact logging: ON") : TEXT("Impact logging: OFF"),
		FLinearColor(0.7f, 0.9f, 0.7f),
		Inner.Min.X + 4.f * VS, Inner.Min.Y + 44.f * VS, Font, 0.8f * VS);
}

void AAfterfallHUD::DrawControlsPage(AAfterfallCharacter* Pawn, const FBox2D& Inner, UFont* Font, float VS)
{
	(void)Pawn; (void)Inner; (void)Font; (void)VS;
	// Rows are buttons drawn by DrawQuickMenu; nothing extra to paint.
}

bool AAfterfallHUD::HandleMenuClick()
{
	APlayerController* PC = GetOwningPlayerController();
	AAfterfallCharacter* Pawn = MenuPawn();
	if (!PC || !Pawn || !Pawn->IsQuickMenuOpen())
	{
		return false;
	}
	float MX = 0.f, MY = 0.f;
	if (!PC->GetMousePosition(MX, MY))
	{
		return true; // menu open: swallow the click even without a cursor pos
	}
	for (int32 i = 0; i < MenuButtons.Num(); ++i)
	{
		const FMenuButton& B = MenuButtons[i];
		if (!B.bEnabled || B.Id == FName(TEXT("Noop")))
		{
			continue;
		}
		if (MX >= B.Rect.Min.X && MX <= B.Rect.Max.X && MY >= B.Rect.Min.Y && MY <= B.Rect.Max.Y)
		{
			Pawn->MenuSelectedIndex = i;
			Pawn->QuickMenuAction(B.Id);
			return true;
		}
	}
	return true; // inside an open menu, clicks never reach the weapon
}

void AAfterfallHUD::ActivateSelectedButton()
{
	if (AAfterfallCharacter* Pawn = MenuPawn())
	{
		if (Pawn->IsQuickMenuOpen() && MenuButtons.IsValidIndex(Pawn->MenuSelectedIndex))
		{
			const FMenuButton& B = MenuButtons[Pawn->MenuSelectedIndex];
			if (B.bEnabled && B.Id != FName(TEXT("Noop")))
			{
				Pawn->QuickMenuAction(B.Id);
			}
		}
	}
}

void AAfterfallHUD::CycleSelection(int32 Delta)
{
	if (AAfterfallCharacter* Pawn = MenuPawn())
	{
		if (Pawn->IsQuickMenuOpen() && MenuButtons.Num() > 0)
		{
		int32 N = MenuButtons.Num();
		int32 I = (Pawn->MenuSelectedIndex + Delta) % N;
		if (I < 0)
		{
			I += N;
		}
		// Skip disabled entries.
		for (int32 K = 0; K < N; ++K)
		{
			if (MenuButtons[I].bEnabled && MenuButtons[I].Id != FName(TEXT("Noop")))
			{
				break;
			}
			I = (I + Delta + N * 8) % N;
		}
		Pawn->MenuSelectedIndex = I;
		}
	}
}
