# Fork diffs (Real Game layering)

Builds use RealGame.uproject. (Afterfall.uproject is the retired fork's project and is kept untouched.)

## Config/DefaultEngine.ini (merged, no overwrite)
- `SecurityToken` in `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]`: fork `00DEFF874304747101E7419B9B83AB26` vs main `CB7540B740E5A3FB3B5F8B87611844C2`. Main wins.
- Every other fork key is identical to the main project's; the merged file equals the main file plus this recorded difference.

## Config/DefaultInput.ini (merged, no overwrite)
- `DefaultTouchInterface`: fork `/Engine/MobileResources/HUD/DefaultVirtualJoysticks.DefaultVirtualJoysticks` vs main `None`. Main wins.
- Fork-only keys added to the merged file: `Gesture_Pinch`, `Gesture_Flick`, `Gesture_Rotate` AxisConfig (3 lines); `bEnabledLegacyMappingDeprecationWarnings`; `bEnablePreferredInputAPIPreferences`; `DefaultPreferredInputAPIList`; `DeprecatedActionAndAxisNames`.
- Main-only keys kept: `ValveIndex_Left_Trackpad_Touch`, `OculusTouch_*` (14), `OculusHand_*` (10), `Cosmos_*` (8) axes; `[EnhancedInputPlatformSettings_Windows EnhancedInputPlatformSettings]` section.
- `Mouse2D` AxisConfig is byte-identical in both (line order only); kept once.
- Fork originals preserved (untracked) at `Docs/_fork_originals/Config/` for the step-5 import review.
