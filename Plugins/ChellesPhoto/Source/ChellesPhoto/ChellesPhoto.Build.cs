using UnrealBuildTool;

public class ChellesPhoto : ModuleRules
{
	public ChellesPhoto(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"ImageWrapper",
			"Projects"
		});
	}
}
