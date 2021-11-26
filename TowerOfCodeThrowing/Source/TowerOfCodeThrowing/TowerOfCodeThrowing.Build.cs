// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TowerOfCodeThrowing : ModuleRules
{
	public TowerOfCodeThrowing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
	}
}
