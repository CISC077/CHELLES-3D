using UnrealBuildTool;

public class ChellesCirculation : ModuleRules
{
	public ChellesCirculation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AssetRegistry",
			"Json"
		});
	}
}
