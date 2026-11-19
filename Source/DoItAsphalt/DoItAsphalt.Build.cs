// Copyright Do it Asphalt. All Rights Reserved.

using UnrealBuildTool;

public class DoItAsphalt : ModuleRules
{
	public DoItAsphalt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Sources include each other relative to the module root ("Game/DIAAssets.h").
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"EngineSettings",
			"InputCore",
			"EnhancedInput",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
			"AsphaltCore"
		});
	}
}
