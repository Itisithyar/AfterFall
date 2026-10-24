// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: UI — minimal survival HUD drawn on Canvas (zero .uasset widgets,
// works in any build). Health + wood + interact prompt + death state.
// Pretty UMG pass can replace this 1:1 later; data stays in components.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AfterfallHUD.generated.h"

UCLASS()
class AFTERFALL_API AAfterfallHUD : public AHUD
{
	GENERATED_BODY()

public:
	AAfterfallHUD();

	/** Route an LMB click to the open quick menu. True = consumed. */
	bool HandleMenuClick();
	/** Gamepad/keyboard confirm on the highlighted button. */
	void ActivateSelectedButton();
	/** Move the highlight (gamepad stick navigation). */
	void CycleSelection(int32 Delta);

protected:
	virtual void DrawHUD() override;
	virtual void Tick(float DeltaTime) override;

private:
	struct FMenuButton
	{
		FName Id;
		FString Label;
		bool bEnabled = true;
		FBox2D Rect;
	};
	TArray<FMenuButton> MenuButtons;
	bool bWasMenuOpen = false;
	double MenuCloseAnimEnd = 0.0;

	class AAfterfallCharacter* MenuPawn() const;
	void DrawQuickMenu(class AAfterfallCharacter* Pawn, UFont* Font, float VS, double Now);
	void DrawMapPage(const FBox2D& Inner, UFont* Font, float VS);
	void DrawControlsPage(class AAfterfallCharacter* Pawn, const FBox2D& Inner, UFont* Font, float VS);
	void DrawLabPage(const FBox2D& Inner, UFont* Font, float VS);
	void DrawMenuButton(int32 Index, const FMenuButton& B, int32 Selected, UFont* Font, float VS, float Alpha);
	void PollCaptureKey(class AAfterfallCharacter* Pawn);
};
