// Copyright Do it Asphalt. All Rights Reserved.

using UnrealBuildTool;

public class DoItAsphalt : ModuleRules
{
	public DoItAsphalt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Slate",
			"SlateCore",
			"AsphaltCore"
		});
	}
}
