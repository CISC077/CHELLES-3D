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
			"PhysXVehicles",
			// 25/11 : plaques CHELLES-3D (widgets UMG du pack DV_Car_Horo)
			"UMG",
			"Slate",
			"SlateCore"
		});
	}
}
