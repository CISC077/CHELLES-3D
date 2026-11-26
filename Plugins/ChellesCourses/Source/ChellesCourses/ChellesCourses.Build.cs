using UnrealBuildTool;

public class ChellesCourses : ModuleRules
{
	public ChellesCourses(ReadOnlyTargetRules Target) : base(Target)
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
