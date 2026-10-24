// Copyright (c) 2026 AFTERFALL. All rights reserved.
using UnrealBuildTool;
using System.Collections.Generic;

public class AfterfallEditorTarget : TargetRules
{
	public AfterfallEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Afterfall");
	}
}
