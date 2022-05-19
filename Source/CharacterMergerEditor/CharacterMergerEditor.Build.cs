// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CharacterMergerEditor : ModuleRules
{
	public CharacterMergerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				/*"CharacterMergerEditor/Public/Editor",
				"CharacterMergerEditor/Public/Subsystem"*/
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EditorStyle",
				"EditorSubsystem",
				"AdvancedPreviewScene",
				"UnrealEd",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"CharacterMerger",
				"SlateCore",
				"Landscape",
				"InputCore",
				"RenderCore",
				"RHI",
				"PlacementMode"
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
