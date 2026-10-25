// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Input) — pure Mouse-1 arbitration rules.
//
// Why this exists: a click must attack, a double-click must rotate the weapon
// hand, and a 0.33 s hold must open the wheel instead of attacking. Those
// three outcomes race each other, so the decisions live here as pure,
// elapsed-time functions that tests can drive without a world or an input
// device. The character owns the timers; this header owns the verdicts.
//
// All inputs are seconds on the same clock (world time), never frame counts,
// so the behavior is frame-rate independent.
#pragma once

#include "CoreMinimal.h"

namespace AFClick
{
	/** Hold reaches the wheel threshold: MenuOpen = t_h >= T_r. */
	FORCEINLINE bool ShouldOpenWheel(double HoldSec, double WheelThresholdSec)
	{
		return HoldSec >= WheelThresholdSec;
	}

	/**
	 * A second press inside the double-click window of the previous RELEASE
	 * converts the pending single click into a rotation press instead of an
	 * attack. Window is measured from release, which is what a player perceives
	 * as "double-clicking".
	 */
	FORCEINLINE bool IsDoubleClick(double PressSec, double PrevReleaseSec,
		double WindowSec, double ResolveDelaySec)
	{
		if (PrevReleaseSec <= 0.0)
		{
			return false;
		}
		const double SinceRelease = PressSec - PrevReleaseSec;
		// Inside the window: second click. Past the resolve delay the first
		// click already fired, so a later press is a fresh first click.
		return SinceRelease >= 0.0 && SinceRelease <= FMath::Min(WindowSec, ResolveDelaySec);
	}

	/**
	 * Release before the wheel threshold is attack-eligible; the attack itself
	 * is deferred until the resolve delay elapses with no second press. A wheel
	 * release is never attack-eligible (the wheel converted the input).
	 */
	FORCEINLINE bool ReleaseIsClick(double HoldSec, double WheelThresholdSec, bool bWheelWasOpen)
	{
		return !bWheelWasOpen && !ShouldOpenWheel(HoldSec, WheelThresholdSec);
	}

	/** The deferred single click fires only while nothing else took over. */
	FORCEINLINE bool ResolvesToAttack(bool bPendingClick, bool bWheelOpen, bool bWeaponRotating)
	{
		return bPendingClick && !bWheelOpen && !bWeaponRotating;
	}
}
