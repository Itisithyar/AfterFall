// Copyright (c) 2026 AFTERFALL. All rights reserved.
using UnrealBuildTool;
using System.Collections.Generic;

public class AfterfallServerTarget : TargetRules
{
	public AfterfallServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Afterfall");
	}
}
