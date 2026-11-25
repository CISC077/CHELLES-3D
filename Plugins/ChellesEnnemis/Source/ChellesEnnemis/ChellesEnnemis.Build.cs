using UnrealBuildTool;

public class ChellesEnnemis : ModuleRules
{
	public ChellesEnnemis(ReadOnlyTargetRules Target) : base(Target)
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
