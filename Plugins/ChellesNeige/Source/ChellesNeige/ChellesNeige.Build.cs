using UnrealBuildTool;

public class ChellesNeige : ModuleRules
{
	public ChellesNeige(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Landscape"
		});
	}
}
