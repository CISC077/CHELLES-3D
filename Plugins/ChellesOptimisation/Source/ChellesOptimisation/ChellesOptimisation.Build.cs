using UnrealBuildTool;

public class ChellesOptimisation : ModuleRules
{
	public ChellesOptimisation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule"
		});
	}
}
