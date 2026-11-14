using UnrealBuildTool;

public class ChellesConduite : ModuleRules
{
	public ChellesConduite(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysXVehicles"
		});
	}
}
