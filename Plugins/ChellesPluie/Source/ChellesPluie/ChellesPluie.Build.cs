using UnrealBuildTool;

public class ChellesPluie : ModuleRules
{
	public ChellesPluie(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Projects",
			"ProceduralMeshComponent"
		});
	}
}
