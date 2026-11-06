using UnrealBuildTool;

public class ChellesPassants : ModuleRules
{
	public ChellesPassants(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"NavigationSystem",
			"AssetRegistry"
		});
	}
}
