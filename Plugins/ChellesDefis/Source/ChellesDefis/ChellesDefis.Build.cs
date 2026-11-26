using UnrealBuildTool;

public class ChellesDefis : ModuleRules
{
	public ChellesDefis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"HTTP",
			"Json"
		});
	}
}
