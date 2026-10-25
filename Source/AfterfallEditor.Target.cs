// Copyright (c) 2026 AFTERFALL. All rights reserved.
using UnrealBuildTool;
using System.Collections.Generic;

public class AfterfallEditorTarget : TargetRules
{
	public AfterfallEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// Match installed UE 5.8 engine build env exactly (shared products with UnrealEditor).
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Afterfall");
	}
}
