using UnrealBuildTool;

public class ChellesVolVoiture : ModuleRules
{
	public ChellesVolVoiture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"PhysXVehicles"
		});
	}
}
