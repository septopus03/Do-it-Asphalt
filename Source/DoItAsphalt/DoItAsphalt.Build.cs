// Copyright Do it Asphalt. All Rights Reserved.

using UnrealBuildTool;

public class DoItAsphalt : ModuleRules
{
	public DoItAsphalt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Every test file keeps its helpers in its own (anonymous) namespace with short names (Move, TickFor, MiddleOfLot ...), which clash
		// as soon as a unity chunk happens to hold two of them; each file is compiled alone instead.
		bUseUnity = false;

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

		// Reads Tools/ModelGen/manifest.json in the model import test.
		PrivateDependencyModuleNames.Add("Json");
	}
}
