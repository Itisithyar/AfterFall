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

protected:
	virtual void DrawHUD() override;
};
