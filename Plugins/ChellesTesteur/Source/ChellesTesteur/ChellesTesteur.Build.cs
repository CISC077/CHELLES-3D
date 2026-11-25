using UnrealBuildTool;

public class ChellesTesteur : ModuleRules
{
	public ChellesTesteur(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RHI",  // nom de la carte graphique pour le rapport
			"Json"  // fichier resultats.json lu par Tools\Testeur\tester_le_jeu.py
		});
	}
}
