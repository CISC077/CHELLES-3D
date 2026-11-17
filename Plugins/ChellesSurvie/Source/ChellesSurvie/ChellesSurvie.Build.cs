using UnrealBuildTool;

public class ChellesSurvie : ModuleRules
{
	public ChellesSurvie(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"InputCore",
			"Slate",
			"SlateCore"
		});
	}
}
