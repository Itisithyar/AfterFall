// Copyright (c) 2026 AFTERFALL. All rights reserved.
using UnrealBuildTool;

public class Afterfall : ModuleRules
{
	public Afterfall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// UE 5.8 installed engine no longer supports Cpp17; match engine standard.
		CppStandard = CppStandardVersion.Cpp20;
		// V7 disables legacy public include paths: subfolder files include
		// module-root headers (Afterfall.h) via explicit module directory.
		PublicIncludePaths.Add(ModuleDirectory);

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
