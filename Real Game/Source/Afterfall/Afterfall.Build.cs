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
			"NetCore",
			"PhysicsCore",
			// UMG + Slate: the Tab inventory mat is a native UUserWidget built
			// from C++ (no .uasset widgets), so the module must link UMG itself.
			"UMG",
			"Slate",
			"SlateCore",
			// Voxel chunk meshes are built at runtime from voxel occupancy
			// (exposed faces only) and need runtime collision built with them.
			// Engine ships this module (Plugin: ProceduralMeshComponent,
			// EnabledByDefault=true); nothing else in the project uses it.
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Explicit ownership: Afterfall module owns Character, Structural, ProcGen stubs.
		// See Docs/ARCHITECTURE.md for module ownership map.
	}
}
