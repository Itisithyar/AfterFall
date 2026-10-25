// Copyright (c) 2026 AFTERFALL. All rights reserved.
using UnrealBuildTool;
using System.Collections.Generic;

public class AfterfallServerTarget : TargetRules
{
	public AfterfallServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		// Match installed UE 5.8 engine build env exactly.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Afterfall");
	}
}
