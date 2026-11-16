using UnrealBuildTool;

public class ChellesPolicier : ModuleRules
{
	public ChellesPolicier(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AssetRegistry",
			"Projects"
		});
	}
}
