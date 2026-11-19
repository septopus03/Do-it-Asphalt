// Copyright Do it Asphalt. All Rights Reserved.

using UnrealBuildTool;

public class AsphaltCore : ModuleRules
{
	public AsphaltCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Pure game logic: depends on Core only.
		PublicDependencyModuleNames.Add("Core");
	}
}
