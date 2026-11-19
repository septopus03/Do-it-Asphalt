// Copyright Do it Asphalt. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DoItAsphaltTarget : TargetRules
{
	public DoItAsphaltTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "AsphaltCore", "DoItAsphalt" });
	}
}
