using UnrealBuildTool;

public class ChellesChargement : ModuleRules
{
	public ChellesChargement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"MoviePlayer",
			"Projects"
		});
	}
}
