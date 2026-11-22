// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VerticalCityFPS : ModuleRules
{
	public VerticalCityFPS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "Json", "RHI", "RenderCore", "ImageWrapper", "AudioMixer", "Niagara" });

		PublicIncludePaths.AddRange(new string[] {
			"VerticalCityFPS",
			"VerticalCityFPS/Variant_Horror",
			"VerticalCityFPS/Variant_Horror/UI",
			"VerticalCityFPS/Variant_Shooter",
			"VerticalCityFPS/Variant_Shooter/AI",
			"VerticalCityFPS/Variant_Shooter/UI",
			"VerticalCityFPS/Variant_Shooter/Weapons"
		});

		RuntimeDependencies.Add("$(ProjectDir)/Content/VerticalCity/Abilities/SkillAtlas.png", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/VerticalCity/Abilities/CombatAtlas.png", StagedFileType.UFS);

		RuntimeDependencies.Add("$(ProjectDir)/Content/VerticalCity/Abilities/S07Casts.bin", StagedFileType.UFS);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
