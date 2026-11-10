using UnrealBuildTool;

public class ChellesPouvoirs : ModuleRules
{
	public ChellesPouvoirs(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"AIModule",
			"AssetRegistry"
		});
	}
}
