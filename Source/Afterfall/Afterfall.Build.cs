// Copyright (c) 2026 AFTERFALL. All rights reserved.
using UnrealBuildTool;

public class Afterfall : ModuleRules
{
	public Afterfall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Explicit ownership: Afterfall module owns Character, Structural, ProcGen stubs.
		// See Docs/ARCHITECTURE.md for module ownership map.
	}
}
