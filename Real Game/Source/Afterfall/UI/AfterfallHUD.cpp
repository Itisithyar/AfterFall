// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Quick menu (Part A): compact top-left Canvas panel. Zero UMG assets so it
// works in every build. Animation uses real time so it plays while paused.
#include "AfterfallHUD.h"
#include "AfterfallCharacter.h"
#include "Combat/HealthComponent.h"
#include "Combat/InjuryStateComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interaction/InteractComponent.h"
#include "UI/RadialMenuComponent.h"
#include "Destruction/FractureSolver.h"
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
	if (!P)
	{
		return;
	}

	// Inventory input takes priority when open (movement/look already gated
	// by the character; the HUD owns UI navigation + close).
	if (P->IsInventoryOpen())
	{
		if (HandleInventoryInput(P))
		{
			return;
		}
	}

	if (!P->IsQuickMenuOpen())
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

	// Contextual radial wheel (crosshair-anchored, Canvas-drawn).
	DrawRadialWheel(Pawn, Font, VS);

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

	// Injury readout: the canonical UInjuryStateComponent ledger is the only
	// source (no second bleeding count). Treatment progress comes from the
	// same replicated session state the server validates.
	if (Pawn->Injury)
	{
		int32 Bleeding = 0;
		int32 Wounded = 0;
		int32 Worst = 0;
		for (const FInjuryRecord& R : Pawn->Injury->PartInjuries)
		{
			if (R.bBleeding) { ++Bleeding; }
			if (R.bWounded) { ++Wounded; }
			Worst = FMath::Max(Worst, R.Severity);
		}
		const float InjuryY = Pad + (BH + 8.f * VS) * 4;
		if (Bleeding > 0)
		{
			DrawText(FString::Printf(TEXT("BLEEDING %d - hold E + LMB"), Bleeding),
				FLinearColor(1.f, 0.15f, 0.15f), Pad, InjuryY, Font, 1.1f * VS);
		}
		else if (Wounded > 0)
		{
			DrawText(FString::Printf(TEXT("WOUNDS %d (severity %d)"), Wounded, Worst),
				FLinearColor(1.f, 0.7f, 0.3f), Pad, InjuryY, Font, 1.1f * VS);
		}
		if (Pawn->Injury->TreatmentState == ETreatmentState::Treating)
		{
			DrawText(FString::Printf(TEXT("Treating %d%%"),
				FMath::RoundToInt(Pawn->Injury->TreatmentProgress * 100.f)),
				FLinearColor(0.6f, 1.f, 0.6f), Pad, InjuryY + 22.f * VS, Font, 1.1f * VS);
		}
	}

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

	// Inventory overlay (Canvas-drawn, zero UMG). Suppressed while the Tab
	// inventory mat widget is up — the mat replaces it 1:1.
	if (Pawn->IsInventoryOpen() && !Pawn->IsInventoryMatOpen())
	{
		DrawInventory(Pawn, Font, VS);
	}
}

void AAfterfallHUD::DrawRadialWheel(AAfterfallCharacter* Pawn, UFont* Font, float VS)
{
	// Visuals only: every value read here is local presentation state owned
	// by URadialMenuComponent (options, selection, radius, dead zone).
	// Confirmation/cancellation stay in the character input controller;
	// this function issues no RPC and mutates no gameplay state.
	if (!Pawn || !Pawn->RadialMenu || !Pawn->RadialMenu->bIsMenuOpen)
	{
		return;
	}
	const URadialMenuComponent* R = Pawn->RadialMenu;
	const int32 N = R->Options.Num();
	if (N <= 0)
	{
		return;
	}

	// Anchor: stored crosshair point, falling back to screen center (the
	// crosshair IS screen center in this project; the camera never moves).
	float CX = (R->MenuCenter.X > 0.f) ? R->MenuCenter.X : Canvas->SizeX * 0.5f;
	float CY = (R->MenuCenter.Y > 0.f) ? R->MenuCenter.Y : Canvas->SizeY * 0.5f;

	// DPI-aware sizing. Assumptions (documented, tunable in one place):
	// 96 px/inch baseline (Windows default), 3-inch wheel diameter, and the
	// project's VS = SizeY/1080 resolution scale. True physical inches vary
	// with panel DPI; PixelsPerInch/WheelUIScale retune per platform.
	// Resulting ring radius: 1080p -> 144px, 1440p -> 192px, 2160p -> 288px.
	const float Radius = R->WheelRadiusPx() * VS;
	const float DeadR = R->SelectionDeadZonePx * VS;
	const float Dot = FMath::Max(2.5f * VS, 2.f);

	// Outer ring (72 dotted segments; DrawRect-only, no texture needed).
	const int32 RingDots = 72;
	for (int32 i = 0; i < RingDots; ++i)
	{
		const float A = (float)i / (float)RingDots * 2.f * PI;
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f),
			CX + FMath::Cos(A) * Radius - Dot * 0.5f,
			CY + FMath::Sin(A) * Radius - Dot * 0.5f, Dot, Dot);
	}

	// Dead-zone ring: inside this radius no sector is selected.
	const int32 DeadDots = 32;
	for (int32 i = 0; i < DeadDots; ++i)
	{
		const float A = (float)i / (float)DeadDots * 2.f * PI;
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.25f),
			CX + FMath::Cos(A) * DeadR - Dot * 0.5f,
			CY + FMath::Sin(A) * DeadR - Dot * 0.5f, Dot, Dot);
	}

	// Sector divider ticks at each boundary. Sector i spans Atan2 angles
	// [i*step - PI, (i+1)*step - PI) with step = 2*PI/N, matching the
	// SectorForDrag contract (sector 0 starts at screen-right, clockwise).
	const float Step = 2.f * PI / (float)N;
	for (int32 i = 0; i < N; ++i)
	{
		const float B = (float)i * Step - PI;
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f),
			CX + FMath::Cos(B) * (Radius + 8.f * VS) - Dot * 0.5f,
			CY + FMath::Sin(B) * (Radius + 8.f * VS) - Dot * 0.5f, Dot, Dot);
	}

	// Selection spoke: dotted line from the dead zone to the ring along the
	// highlighted sector's mid-angle, so the current choice reads at a glance.
	if (R->SelectedOption >= 0 && R->SelectedOption < N)
	{
		const float Mid = ((float)R->SelectedOption + 0.5f) * Step - PI;
		const float DX = FMath::Cos(Mid), DY = FMath::Sin(Mid);
		const int32 SpokeDots = 16;
		for (int32 i = 0; i <= SpokeDots; ++i)
		{
			const float L = DeadR + (Radius - DeadR) * (float)i / (float)SpokeDots;
			DrawRect(FLinearColor(1.f, 0.9f, 0.3f, 0.9f),
				CX + DX * L - Dot * 0.5f, CY + DY * L - Dot * 0.5f, Dot, Dot);
		}
	}

	// Labels at each sector mid-angle, just outside the ring. Selected =
	// yellow, disabled = dimmed (server still has the final say on tap).
	for (int32 i = 0; i < N; ++i)
	{
		const FRadialMenuOption& Opt = R->Options[i];
		const float Mid = ((float)i + 0.5f) * Step - PI;
		const float L = Radius + 34.f * VS;
		const FString Label = Opt.DisplayName.ToString();
		float TW = 0.f, TH = 0.f;
		Canvas->TextSize(Font, Label, TW, TH, 1.1f * VS, 1.1f * VS);
		const FLinearColor Col = !Opt.bIsEnabled ? FLinearColor(0.5f, 0.5f, 0.5f, 0.8f)
			: (i == R->SelectedOption ? FLinearColor(1.f, 0.9f, 0.3f) : FLinearColor::White);
		DrawText(Label, Col, CX + FMath::Cos(Mid) * L - TW * 0.5f,
			CY + FMath::Sin(Mid) * L - TH * 0.5f, Font, 1.1f * VS);
	}

	// One-line hint under the wheel. The game never takes the cursor for
	// the wheel, so there is no cursor/input state to restore on close.
	const FString Hint = TEXT("release LMB to confirm - RMB / Esc cancels");
	float HW = 0.f, HH = 0.f;
	Canvas->TextSize(Font, Hint, HW, HH, 0.9f * VS, 0.9f * VS);
	DrawText(Hint, FLinearColor(1.f, 1.f, 1.f, 0.7f),
		CX - HW * 0.5f, CY + Radius + 64.f * VS, Font, 0.9f * VS);
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
		ExtraH = 132.f * VS; // summary + log + quadratic tuning rows
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

	// Quadratic severity tuning (single tunable instance in FractureSolver).
	// Display-only: values are set via AfterfallFracture::SetSeverityResponse;
	// the lab page exposes current state + the derived initiation energy.
	const AfterfallFracture::FFractureSeverityResponse& QR = AfterfallFracture::GetSeverityResponse();
	const float XStar = AfterfallFracture::InitiationSeverityX(QR);
	DrawText(FString::Printf(TEXT("Quad K=%.0f Eref=%.0fJ Dthr=%.0f"),
		QR.K, QR.ReferenceEnergyJ, QR.InitiationThreshold),
		FLinearColor(0.7f, 0.8f, 1.f),
		Inner.Min.X + 4.f * VS, Inner.Min.Y + 64.f * VS, Font, 0.8f * VS);
	DrawText(FString::Printf(TEXT("x*=%.2f Einits=%.0fJ (gates)"),
		XStar, XStar * QR.ReferenceEnergyJ),
		FLinearColor(0.7f, 0.8f, 1.f),
		Inner.Min.X + 4.f * VS, Inner.Min.Y + 86.f * VS, Font, 0.8f * VS);
	DrawText(FString::Printf(TEXT("CapJ P%.0f W%.0f St%.0f M%.0f C%.0f"),
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Plaster),
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Wood),
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Stone),
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Metal),
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Concrete)),
		FLinearColor(0.9f, 0.8f, 0.6f),
		Inner.Min.X + 4.f * VS, Inner.Min.Y + 108.f * VS, Font, 0.8f * VS);
}

void AAfterfallHUD::DrawControlsPage(AAfterfallCharacter* Pawn, const FBox2D& Inner, UFont* Font, float VS)
{
	(void)Pawn; (void)Inner; (void)Font; (void)VS;
	// Rows are buttons drawn by DrawQuickMenu; nothing extra to paint.
}

// ---------------------------------------------------------------- Inventory UI ---

void AAfterfallHUD::DrawInventory(AAfterfallCharacter* Pawn, UFont* Font, float VS)
{
	if (!Pawn || !Pawn->Inventory || !Canvas)
	{
		return;
	}

	const float Pad = 24.f * VS;
	const float PanelW = 960.f * VS;
	const float PanelH = 640.f * VS;
	const float PanelX = (Canvas->SizeX - PanelW) * 0.5f;
	const float PanelY = (Canvas->SizeY - PanelH) * 0.5f;

	DrawRect(FLinearColor(0.05f, 0.06f, 0.08f, 0.95f), PanelX, PanelY, PanelW, PanelH);
	DrawRect(FLinearColor(0.4f, 0.45f, 0.55f, 0.9f), PanelX, PanelY, PanelW, 1.f);
	DrawRect(FLinearColor(0.4f, 0.45f, 0.55f, 0.9f), PanelX, PanelY + PanelH - 1.f, PanelW, 1.f);

	DrawText(TEXT("INVENTORY"), FLinearColor::White, PanelX + Pad, PanelY + Pad, Font, 1.2f * VS);

	// Category tabs.
	const TArray<FName> Categories = {
		FName(TEXT("All")), FName(TEXT("Weapons")), FName(TEXT("Equipment")),
		FName(TEXT("Medical")), FName(TEXT("Resources")), FName(TEXT("Food & Drink")),
		FName(TEXT("Quest"))
	};
	const float TabW = 110.f * VS;
	const float TabH = 30.f * VS;
	float TabX = PanelX + Pad;
	for (int32 i = 0; i < Categories.Num(); ++i)
	{
		const bool bSelected = (InventorySelectedCategory == i);
		DrawRect(bSelected ? FLinearColor(0.25f, 0.45f, 0.75f, 0.95f) : FLinearColor(0.16f, 0.18f, 0.22f, 0.9f),
			TabX, PanelY + Pad + 30.f, TabW, TabH);
		DrawText(Categories[i].ToString(), bSelected ? FLinearColor::White : FLinearColor(0.8f, 0.8f, 0.8f),
			TabX + 8.f * VS, PanelY + Pad + 34.f * VS, Font, 1.0f * VS);
		TabX += TabW + 8.f * VS;
	}

	// Item grid.
	const int32 Cols = 4;
	const int32 Rows = 6;
	const float SlotW = 70.f * VS;
	const float SlotH = 70.f * VS;
	const float GridX = PanelX + Pad;
	const float GridY = PanelY + Pad + 70.f * VS;

	EItemCategory FilterCat = (InventorySelectedCategory == 0) ? EItemCategory::All :
		static_cast<EItemCategory>(InventorySelectedCategory);

	TArray<FInventorySlot> Filtered = Pawn->Inventory->GetItemsByCategory(FilterCat);

	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			int32 SlotIdx = r * Cols + c;
			float SX = GridX + c * (SlotW + 8.f * VS);
			float SY = GridY + r * (SlotH + 8.f * VS);

			FInventorySlot* Slot = (SlotIdx < Filtered.Num()) ? &Filtered[SlotIdx] : nullptr;
			const bool bSelected = (InventorySelectedSlot == SlotIdx);
			const bool bHovered = (InventoryHoveredSlot == SlotIdx);
			const bool bHasItem = Slot && !Slot->IsEmpty();

			DrawRect(bSelected ? FLinearColor(0.35f, 0.55f, 0.85f, 0.95f)
				: (bHovered ? FLinearColor(0.25f, 0.28f, 0.35f, 0.9f)
				: (bHasItem ? FLinearColor(0.12f, 0.14f, 0.18f, 0.9f)
				: FLinearColor(0.08f, 0.10f, 0.14f, 0.8f))),
				SX, SY, SlotW, SlotH);

			if (bHasItem)
			{
				FLinearColor IconCol = FLinearColor::White;
				EItemCategory IconCat = Slot->Item.ItemId.IsEqual(TEXT("AF_MED_Bandage"))
					? EItemCategory::Medical : FilterCat;
				if (IconCat == EItemCategory::Weapons) IconCol = FLinearColor(1.f, 0.4f, 0.4f);
				else if (IconCat == EItemCategory::Equipment) IconCol = FLinearColor(0.4f, 1.f, 0.4f);
				else if (IconCat == EItemCategory::Medical) IconCol = FLinearColor(0.4f, 0.8f, 1.f);
				else if (IconCat == EItemCategory::Resources) IconCol = FLinearColor(1.f, 0.8f, 0.4f);
				else if (IconCat == EItemCategory::FoodDrink) IconCol = FLinearColor(1.f, 1.f, 0.4f);
				else if (IconCat == EItemCategory::Quest) IconCol = FLinearColor(1.f, 0.4f, 1.f);
				else IconCol = FLinearColor(0.8f, 0.8f, 0.8f);
				DrawRect(IconCol, SX + 10.f * VS, SY + 10.f * VS, SlotW - 20.f * VS, SlotH - 20.f * VS);

				FString ItemName = Slot->Item.ItemId.ToString();
				if (ItemName.Len() > 12) ItemName = ItemName.Left(10) + TEXT("..");
				DrawText(ItemName, FLinearColor::White, SX + 4.f * VS, SY + 4.f * VS, Font, 0.8f * VS);

				if (Slot->Item.Count > 1)
				{
					DrawText(FString::Printf(TEXT("x%d"), Slot->Item.Count), FLinearColor::Yellow,
						SX + SlotW - 20.f * VS, SY + SlotH - 14.f * VS, Font, 0.9f * VS);
				}
			}
			else
			{
				DrawText(TEXT("-"), FLinearColor(0.4f, 0.4f, 0.4f),
					SX + SlotW * 0.5f - 4.f * VS, SY + SlotH * 0.5f - 8.f * VS, Font, 1.0f * VS);
			}
		}
	}

	// Selected item details panel (right side).
	float DetailX = PanelX + PanelW - Pad - 260.f * VS;
	float DetailY = GridY;
	float DetailW = 260.f * VS;
	DrawRect(FLinearColor(0.08f, 0.10f, 0.14f, 0.95f), DetailX, DetailY, DetailW, PanelH - 70.f * VS);
	DrawRect(FLinearColor(0.4f, 0.45f, 0.55f, 0.9f), DetailX, DetailY, DetailW, 1.f);

	if (InventorySelectedSlot >= 0 && InventorySelectedSlot < Filtered.Num())
	{
		const FInventorySlot& Slot = Filtered[InventorySelectedSlot];
		if (!Slot.IsEmpty())
		{
			DrawText(Slot.Item.ItemId.ToString(), FLinearColor::White, DetailX + Pad, DetailY + Pad, Font, 1.1f * VS);
			DrawText(FString::Printf(TEXT("Count: %d"), Slot.Item.Count), FLinearColor::White,
				DetailX + Pad, DetailY + Pad + 24.f * VS, Font, 1.0f * VS);
			DrawText(FString::Printf(TEXT("Condition: %.0f"), Slot.Item.Condition * 100.f), FLinearColor::White,
				DetailX + Pad, DetailY + Pad + 40.f * VS, Font, 1.0f * VS);

			float BtnY = DetailY + Pad + 80.f * VS;
			float BtnW = DetailW - Pad * 2.f;
			float BtnH = 28.f * VS;

			FString ActionLabel = TEXT("Use");
			UItemDefinition* Def = Pawn->Inventory->GetItemDefinition(Slot.Item.ItemId);
			if (Def && Def->bEquippable) ActionLabel = TEXT("Equip");
			else if (!Def->MedicalItemId.IsNone()) ActionLabel = TEXT("Use (Treat)");

			DrawRect(FLinearColor(0.25f, 0.45f, 0.75f, 0.95f), DetailX + Pad, BtnY, BtnW, BtnH);
			DrawText(ActionLabel, FLinearColor::White, DetailX + Pad + 8.f * VS, BtnY + 4.f * VS, Font, 1.0f * VS);

			BtnY += BtnH + 8.f * VS;
			DrawRect(FLinearColor(0.6f, 0.2f, 0.2f, 0.95f), DetailX + Pad, BtnY, BtnW, BtnH);
			DrawText(TEXT("Drop"), FLinearColor::White, DetailX + Pad + 8.f * VS, BtnY + 4.f * VS, Font, 1.0f * VS);
		}
	}

	float HintY = PanelY + PanelH - Pad - 20.f * VS;
	DrawText(TEXT("Tab/Esc=Close  Arrows=Navigate  E=Use  G=Drop"), FLinearColor(0.7f, 0.7f, 0.7f),
		PanelX + Pad, HintY, Font, 0.9f * VS);
}

bool AAfterfallHUD::HandleInventoryInput(AAfterfallCharacter* Pawn)
{
	if (!Pawn || !Pawn->IsInventoryOpen())
	{
		return false;
	}
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return false;
	}

	// Tab is handled ONCE by the Enhanced Input binding (IA_Inventory ->
	// OnInventoryPressed -> ToggleInventory). Handling it here as well made a
	// single press toggle twice in the same frame (open then close), which is
	// why Tab looked broken. Escape stays as the HUD-side way out.
	if (PC->WasInputKeyJustPressed(EKeys::Escape))
	{
		Pawn->CloseInventory();
		InventorySelectedSlot = -1;
		InventoryHoveredSlot = -1;
		return true;
	}

	// The mat widget owns slot selection (mouse click, select/swap) while it
	// is up: swallow the canvas inventory's nav/use/drop keys so the two
	// interfaces cannot act on different inventories at once.
	if (Pawn->IsInventoryMatOpen())
	{
		return true;
	}

	double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - InventoryLastNavTime < 0.15)
	{
		return true;
	}

	if (PC->WasInputKeyJustPressed(EKeys::Left) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left))
	{
		InventoryNavigate(-1, 0);
		InventoryLastNavTime = Now;
		return true;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Right) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right))
	{
		InventoryNavigate(1, 0);
		InventoryLastNavTime = Now;
		return true;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))
	{
		InventoryNavigate(0, -1);
		InventoryLastNavTime = Now;
		return true;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Down) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down))
	{
		InventoryNavigate(0, 1);
		InventoryLastNavTime = Now;
		return true;
	}

	if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
	{
		if (InventorySelectedSlot >= 0)
		{
			Pawn->ServerUseInventorySlot(InventorySelectedSlot);
		}
		return true;
	}

	if (PC->WasInputKeyJustPressed(EKeys::G))
	{
		if (InventorySelectedSlot >= 0)
		{
			Pawn->ServerDropInventorySlot(InventorySelectedSlot, 1);
		}
		return true;
	}

	return false;
}

void AAfterfallHUD::InventoryNavigate(int32 DeltaX, int32 DeltaY)
{
	if (DeltaX != 0)
	{
		InventorySelectedSlot += DeltaX;
		if (InventorySelectedSlot < 0) InventorySelectedSlot = 0;
		if (InventorySelectedSlot > 23) InventorySelectedSlot = 23;
	}
	if (DeltaY != 0)
	{
		InventorySelectedSlot += DeltaY * 4;
		if (InventorySelectedSlot < 0) InventorySelectedSlot = 0;
		if (InventorySelectedSlot > 23) InventorySelectedSlot = 23;
	}
	InventoryHoveredSlot = InventorySelectedSlot;
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
