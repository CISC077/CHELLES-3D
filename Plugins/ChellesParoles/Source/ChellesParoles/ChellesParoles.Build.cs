using UnrealBuildTool;

public class ChellesParoles : ModuleRules
{
	public ChellesParoles(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Projects",
			"Slate",
			"SlateCore"
		});
	}
}
