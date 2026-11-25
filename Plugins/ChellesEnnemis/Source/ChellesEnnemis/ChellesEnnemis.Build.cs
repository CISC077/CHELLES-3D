using UnrealBuildTool;

public class ChellesEnnemis : ModuleRules
{
	public ChellesEnnemis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// 25/11 : BotsCombinaison.cpp et EnnemisChelles.cpp ont les memes petites fonctions internes :
		// sans ce reglage, Unreal les colle dans un seul fichier et la compilation echoue (redefinition).
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"NavigationSystem",
			"AssetRegistry"
		});
	}
}
